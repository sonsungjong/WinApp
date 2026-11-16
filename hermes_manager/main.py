"""Hermes Studio Manager Backend (FastAPI on 0.0.0.0:45501).

Provides live streaming chat over Gateway RPC (session.create, session.resume,
prompt.submit, session.interrupt, session.mutate), dynamic Codex OAuth model
catalog enrichment (.codex/models_cache.json + Hermes /api/model/options),
state.db message/tool-call inspection, workspace file browsing, and MCP control.
"""

from __future__ import annotations

import asyncio
import contextlib
import json
import os
import sqlite3
import sys
import uuid
from pathlib import Path
from typing import Any, Optional

HERMES_AGENT_DIR = Path(r"C:\Users\cbmplus\AppData\Local\hermes\hermes-agent")
if str(HERMES_AGENT_DIR) not in sys.path:
    sys.path.insert(0, str(HERMES_AGENT_DIR))

import hermes_bootstrap  # noqa: F401, E402
import httpx  # noqa: E402
import uvicorn  # noqa: E402
from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect  # noqa: E402
from fastapi.middleware.cors import CORSMiddleware  # noqa: E402
from pydantic import BaseModel, Field  # noqa: E402

from hermes_cli.config import load_config  # noqa: E402
from hermes_cli.gateway_client import GatewayClientError, connect_gateway  # noqa: E402
from hermes_cli.gateway_runtime import discover_gateway_endpoint  # noqa: E402
from hermes_constants import VALID_REASONING_EFFORTS, get_hermes_home  # noqa: E402

HERMES_HOME = Path(get_hermes_home())
HERMES_WORKSPACE = HERMES_HOME / "workspace"
WINAPP_ROOT = Path(r"C:\Users\cbmplus\source\WinApp")
CODEX_MODELS_CACHE = Path(os.environ.get("CODEX_HOME", "").strip() or (Path.home() / ".codex")) / "models_cache.json"
HERMES_API_ORIGIN = "http://127.0.0.1:9120"
HERMES_TOKEN_PATH = HERMES_HOME / "webapp" / "session-token"
HERMES_DB_PATH = HERMES_HOME / "state.db"
NODE_MCP_ENTRY = WINAPP_ROOT / "NodeMcpServer" / "dist" / "index.js"
NODE_MCP_NAME = "node_winapp_mcp"

WORKSPACE_ROOTS: dict[str, Path] = {
    "winapp": WINAPP_ROOT.resolve(),
    "hermes": HERMES_WORKSPACE.resolve(),
}

_ws_clients: set[WebSocket] = set()
_ws_clients_lock = asyncio.Lock()
_active_turns: dict[str, dict[str, Any]] = {}


class ModelApplyRequest(BaseModel):
    provider: str = "openai-codex"
    model: str
    reasoning_effort: Optional[str] = None
    apply_to_active_sessions: bool = True


class SessionModelRequest(BaseModel):
    provider: str = "openai-codex"
    model: str
    reasoning_effort: Optional[str] = None


class CreateSessionRequest(BaseModel):
    title: Optional[str] = None
    provider: Optional[str] = None
    model: Optional[str] = None
    reasoning_effort: Optional[str] = None


class SendMessageRequest(BaseModel):
    text: str


class McpToolCallRequest(BaseModel):
    tool_name: str
    arguments: dict[str, Any] = Field(default_factory=dict)


async def _ws_broadcast(message: dict[str, Any]) -> None:
    """Broadcast a JSON message to all connected WebSocket clients."""
    async with _ws_clients_lock:
        clients = list(_ws_clients)
    if not clients:
        return
    encoded = json.dumps(message, ensure_ascii=False)
    stale: list[WebSocket] = []
    for ws in clients:
        try:
            await ws.send_text(encoded)
        except Exception:
            stale.append(ws)
    if stale:
        async with _ws_clients_lock:
            for ws in stale:
                _ws_clients.discard(ws)


async def _periodic_ws_broadcaster() -> None:
    """Push periodic overview updates every 4s while WebSocket clients are connected."""
    while True:
        await asyncio.sleep(4.0)
        async with _ws_clients_lock:
            has_clients = bool(_ws_clients)
        if not has_clients:
            continue
        try:
            overview = await get_overview()
            await _ws_broadcast({"type": "overview", "overview": overview})
        except Exception as exc:
            await _ws_broadcast({"type": "error", "error": str(exc)})


@contextlib.asynccontextmanager
async def _lifespan(_app: FastAPI):
    task = asyncio.create_task(_periodic_ws_broadcaster())
    try:
        yield
    finally:
        task.cancel()
        for entry in list(_active_turns.values()):
            turn_task = entry.get("task")
            if turn_task and not turn_task.done():
                turn_task.cancel()
        with contextlib.suppress(asyncio.CancelledError):
            await task


app = FastAPI(title="Hermes Studio Manager API", version="2.0.0", lifespan=_lifespan)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


def _hermes_headers() -> dict[str, str]:
    if not HERMES_TOKEN_PATH.exists():
        raise HTTPException(status_code=503, detail=f"Hermes session token not found at {HERMES_TOKEN_PATH}")
    token = HERMES_TOKEN_PATH.read_text(encoding="utf-8").strip()
    return {"Authorization": f"Bearer {token}"}


async def _hermes_request(
    method: str,
    path: str,
    *,
    json_body: Any = None,
    params: Optional[dict[str, Any]] = None,
    timeout: float = 25.0,
) -> Any:
    async with httpx.AsyncClient(base_url=HERMES_API_ORIGIN, timeout=timeout) as client:
        try:
            resp = await client.request(
                method,
                path,
                headers=_hermes_headers(),
                json=json_body,
                params=params,
            )
        except httpx.RequestError as exc:
            raise HTTPException(status_code=503, detail=f"Failed to reach Hermes API at {HERMES_API_ORIGIN}: {exc}") from exc
    if resp.status_code >= 400:
        detail: Any
        try:
            detail = resp.json()
        except ValueError:
            detail = resp.text
        raise HTTPException(status_code=resp.status_code, detail=detail)
    return resp.json()


def _query_db(fn):
    if not HERMES_DB_PATH.exists():
        return None
    uri = f"file:{HERMES_DB_PATH.as_posix()}?mode=ro"
    conn = sqlite3.connect(uri, uri=True, timeout=5.0)
    conn.row_factory = sqlite3.Row
    try:
        return fn(conn)
    finally:
        conn.close()


def _parse_json_dict(raw: Any) -> dict[str, Any]:
    if not raw or not isinstance(raw, str):
        return {}
    try:
        parsed = json.loads(raw)
        return parsed if isinstance(parsed, dict) else {}
    except json.JSONDecodeError:
        return {}


def _parse_json_list(raw: Any) -> list[Any]:
    if not raw or not isinstance(raw, str):
        return []
    try:
        parsed = json.loads(raw)
        return parsed if isinstance(parsed, list) else []
    except json.JSONDecodeError:
        return []


def _load_codex_cache_metadata() -> dict[str, Any]:
    """Read live Codex OAuth catalog cache from ~/.codex/models_cache.json."""
    if not CODEX_MODELS_CACHE.exists():
        return {"fetched_at": None, "client_version": None, "by_slug": {}, "models": []}
    try:
        raw = json.loads(CODEX_MODELS_CACHE.read_text(encoding="utf-8-sig"))
    except Exception:
        return {"fetched_at": None, "client_version": None, "by_slug": {}, "models": []}

    by_slug: dict[str, dict[str, Any]] = {}
    for entry in raw.get("models") or []:
        if not isinstance(entry, dict):
            continue
        slug = str(entry.get("slug") or "").strip()
        if not slug:
            continue
        levels = [
            {
                "effort": str(lvl.get("effort") or ""),
                "description": str(lvl.get("description") or ""),
            }
            for lvl in (entry.get("supported_reasoning_levels") or [])
            if isinstance(lvl, dict) and lvl.get("effort")
        ]
        by_slug[slug] = {
            "slug": slug,
            "display_name": str(entry.get("display_name") or slug),
            "description": str(entry.get("description") or ""),
            "default_reasoning_level": str(entry.get("default_reasoning_level") or "medium"),
            "supported_reasoning_levels": levels,
            "visibility": str(entry.get("visibility") or "list"),
            "context_window": entry.get("context_window"),
        }
    return {
        "fetched_at": raw.get("fetched_at"),
        "client_version": raw.get("client_version"),
        "by_slug": by_slug,
    }


def _build_model_detail(slug: str, provider_slug: str, codex_by_slug: dict[str, dict[str, Any]]) -> dict[str, Any]:
    is_900k = slug.endswith("-900k")
    base_slug = slug[:-5] if is_900k else slug
    codex_entry = codex_by_slug.get(slug) or codex_by_slug.get(base_slug)
    if codex_entry:
        display_name = codex_entry["display_name"]
        if is_900k and not display_name.lower().endswith("900k"):
            display_name = f"{display_name} (900K Context)"
        desc = codex_entry["description"]
        if is_900k:
            desc = f"{desc} · Large 900K context window variant".strip(" ·")
        return {
            "id": slug,
            "display_name": display_name,
            "description": desc,
            "default_reasoning_level": codex_entry["default_reasoning_level"],
            "supported_reasoning_levels": codex_entry["supported_reasoning_levels"],
            "visibility": codex_entry["visibility"],
            "context_window": 900000 if is_900k else (codex_entry.get("context_window") or 272000),
            "source": "codex-oauth-live-catalog",
        }
    return {
        "id": slug,
        "display_name": slug,
        "description": f"{provider_slug} catalog model",
        "default_reasoning_level": "medium",
        "supported_reasoning_levels": [{"effort": e, "description": ""} for e in VALID_REASONING_EFFORTS],
        "visibility": "list",
        "context_window": 900000 if is_900k else None,
        "source": "hermes-provider-catalog",
    }


def _load_sessions_snapshot(limit: int = 30) -> list[dict[str, Any]]:
    def _read(conn: sqlite3.Connection) -> list[dict[str, Any]]:
        routing_rows = conn.execute("SELECT session_key, entry_json FROM gateway_routing").fetchall()
        routing_map = {r["session_key"]: _parse_json_dict(r["entry_json"]) for r in routing_rows}

        rows = conn.execute(
            """
            SELECT id, source, session_key, display_name, title, model, model_config,
                   started_at, ended_at, last_activity_at, message_count, api_call_count,
                   input_tokens, output_tokens, cache_read_tokens, reasoning_tokens,
                   runtime_revision, runtime_generation, billing_provider, cwd
            FROM sessions
            ORDER BY COALESCE(last_activity_at, started_at) DESC
            LIMIT ?
            """,
            (limit,),
        ).fetchall()

        sessions: list[dict[str, Any]] = []
        for r in rows:
            skey = r["session_key"] or ""
            route_entry = routing_map.get(skey, {})
            model_cfg = _parse_json_dict(r["model_config"])
            override = route_entry.get("model_override") if isinstance(route_entry.get("model_override"), dict) else {}
            gateway_rt = model_cfg.get("gateway_runtime") if isinstance(model_cfg.get("gateway_runtime"), dict) else {}
            effective_model = override.get("model") or model_cfg.get("model") or r["model"] or ""
            effective_provider = (
                override.get("provider")
                or model_cfg.get("provider")
                or gateway_rt.get("provider")
                or r["billing_provider"]
                or ""
            )
            sid = r["id"]
            sessions.append(
                {
                    "id": sid,
                    "source": r["source"],
                    "session_key": skey,
                    "display_name": r["display_name"],
                    "title": r["title"],
                    "stored_model": r["model"],
                    "effective_model": effective_model,
                    "effective_provider": effective_provider,
                    "has_override": bool(override.get("model")),
                    "override_model": override.get("model"),
                    "started_at": r["started_at"],
                    "ended_at": r["ended_at"],
                    "is_active": r["ended_at"] is None,
                    "is_streaming": sid in _active_turns,
                    "last_activity_at": r["last_activity_at"],
                    "message_count": r["message_count"] or 0,
                    "api_call_count": r["api_call_count"] or 0,
                    "input_tokens": r["input_tokens"] or 0,
                    "output_tokens": r["output_tokens"] or 0,
                    "cache_read_tokens": r["cache_read_tokens"] or 0,
                    "reasoning_tokens": r["reasoning_tokens"] or 0,
                    "runtime_revision": r["runtime_revision"] or 0,
                    "runtime_generation": r["runtime_generation"],
                    "cwd": r["cwd"],
                }
            )
        return sessions

    return _query_db(_read) or []


def _load_session_messages_sync(session_id: str, limit: int = 80) -> list[dict[str, Any]]:
    def _read(conn: sqlite3.Connection) -> list[dict[str, Any]]:
        rows = conn.execute(
            """
            SELECT id, role, content, tool_name, tool_calls, tool_call_id,
                   reasoning, reasoning_content, display_kind, timestamp
            FROM messages
            WHERE session_id = ?
            ORDER BY id DESC
            LIMIT ?
            """,
            (session_id, limit),
        ).fetchall()
        items = []
        for r in rows:
            raw_tool_calls = _parse_json_list(r["tool_calls"])
            parsed_calls = []
            for tc in raw_tool_calls:
                if not isinstance(tc, dict):
                    continue
                fn = tc.get("function") if isinstance(tc.get("function"), dict) else {}
                parsed_calls.append(
                    {
                        "id": tc.get("id") or tc.get("call_id") or "",
                        "name": fn.get("name") or tc.get("name") or "",
                        "arguments": fn.get("arguments") or tc.get("arguments") or "",
                    }
                )
            items.append(
                {
                    "id": r["id"],
                    "role": r["role"],
                    "content": r["content"] or "",
                    "tool_name": r["tool_name"],
                    "tool_calls": parsed_calls,
                    "tool_call_id": r["tool_call_id"],
                    "reasoning": r["reasoning"] or r["reasoning_content"] or "",
                    "display_kind": r["display_kind"],
                    "timestamp": r["timestamp"],
                }
            )
        items.reverse()
        return items

    return _query_db(_read) or []


async def _mutate_session_via_gateway(
    session_id: str,
    expected_revision: int,
    expected_generation: Optional[int],
    provider: str,
    model: str,
    reasoning_effort: Optional[str] = None,
) -> dict[str, Any]:
    discovery = await asyncio.to_thread(discover_gateway_endpoint, HERMES_HOME, timeout=2.0)
    if discovery.endpoint is None:
        raise HTTPException(status_code=503, detail=f"Gateway endpoint unavailable: {discovery.state}")

    payload: dict[str, Any] = {"model": model, "provider": provider}
    if reasoning_effort and reasoning_effort.strip():
        payload["reasoning"] = reasoning_effort.strip().lower()

    call_base: dict[str, Any] = {
        "session_id": session_id,
        "expected_revision": expected_revision,
        "operation": "model",
    }
    if expected_generation is not None:
        call_base["expected_generation"] = expected_generation

    async with connect_gateway(endpoint=discovery.endpoint) as client:
        res = await client.rpc("session.mutate", request_id=uuid.uuid4().hex, payload=payload, **call_base)
        if isinstance(res, dict) and res.get("status") == "confirmation_required" and res.get("confirm"):
            confirmed_payload = {**payload, "confirm": res["confirm"]}
            res = await client.rpc(
                "session.mutate",
                request_id=uuid.uuid4().hex,
                payload=confirmed_payload,
                **call_base,
            )
        return res if isinstance(res, dict) else {"result": res}


async def _run_streaming_turn(session_id: str, text: str) -> None:
    """Attach to session via Gateway WS, submit prompt, and relay live events to WebUI clients."""
    discovery = await asyncio.to_thread(discover_gateway_endpoint, HERMES_HOME, timeout=2.0)
    if discovery.endpoint is None:
        await _ws_broadcast(
            {"type": "chat_event", "session_id": session_id, "event_type": "turn.error", "payload": {"error": "Gateway unavailable"}}
        )
        return

    try:
        async with connect_gateway(endpoint=discovery.endpoint) as client:
            _active_turns[session_id]["client"] = client
            resume_res = await client.rpc("session.resume", session_id=session_id, omit_messages=True)
            subscription_id = resume_res.get("subscription_id") if isinstance(resume_res, dict) else None
            receipt = await client.rpc(
                "prompt.submit",
                session_id=session_id,
                text=text,
                submission_id=uuid.uuid4().hex,
            )
            admission_id = receipt.get("admission_id") if isinstance(receipt, dict) else None
            await _ws_broadcast(
                {
                    "type": "chat_event",
                    "session_id": session_id,
                    "event_type": "turn.started",
                    "payload": {"admission_id": admission_id, "text": text},
                }
            )

            while True:
                event = await asyncio.wait_for(client.events.get(), timeout=600.0)
                if isinstance(event, Exception):
                    raise event
                if not isinstance(event, dict):
                    continue
                params = event.get("params")
                if not isinstance(params, dict):
                    continue
                if params.get("session_id") != session_id:
                    continue
                kind = str(params.get("type") or "")
                payload = params.get("payload") if isinstance(params.get("payload"), dict) else {}
                ev_admission = params.get("admission_id") or payload.get("admission_id")
                if admission_id and ev_admission and ev_admission != admission_id:
                    continue

                await _ws_broadcast(
                    {
                        "type": "chat_event",
                        "session_id": session_id,
                        "event_type": kind,
                        "payload": payload,
                    }
                )
                if kind in {"message.complete", "turn.error"}:
                    break

            if subscription_id:
                with contextlib.suppress(Exception):
                    await client.rpc("session.detach", session_id=session_id, subscription_id=subscription_id)
    except asyncio.CancelledError:
        pass
    except Exception as exc:
        await _ws_broadcast(
            {
                "type": "chat_event",
                "session_id": session_id,
                "event_type": "turn.error",
                "payload": {"error": str(exc)},
            }
        )
    finally:
        _active_turns.pop(session_id, None)
        messages = await asyncio.to_thread(_load_session_messages_sync, session_id, 80)
        await _ws_broadcast({"type": "messages_update", "session_id": session_id, "messages": messages})
        await _broadcast_full_overview()


@app.get("/api/overview")
async def get_overview():
    status_data, model_info, cfg, sessions = await asyncio.gather(
        _hermes_request("GET", "/api/status"),
        _hermes_request("GET", "/api/model/info"),
        asyncio.to_thread(load_config),
        asyncio.to_thread(_load_sessions_snapshot, 30),
    )
    model_cfg = cfg.get("model") if isinstance(cfg.get("model"), dict) else {}
    agent_cfg = cfg.get("agent") if isinstance(cfg.get("agent"), dict) else {}
    telegram_session = next((s for s in sessions if s["source"] == "telegram" and s["is_active"]), None)
    if telegram_session is None:
        telegram_session = next((s for s in sessions if s["source"] == "telegram"), None)

    return {
        "hermes_home": str(HERMES_HOME),
        "gateway": {
            "running": status_data.get("gateway_running"),
            "state": status_data.get("gateway_state"),
            "version": status_data.get("version"),
            "platforms": status_data.get("gateway_platforms", {}),
        },
        "global_model": {
            "provider": model_cfg.get("provider") or model_info.get("provider") or "",
            "model": model_cfg.get("default") or model_info.get("model") or "",
            "reasoning_effort": agent_cfg.get("reasoning_effort") or "",
            "effective_context_length": model_info.get("effective_context_length", 0),
        },
        "telegram_session": telegram_session,
        "sessions": sessions,
    }


@app.get("/api/models")
async def get_models(refresh: bool = False):
    options, codex_cache = await asyncio.gather(
        _hermes_request("GET", "/api/model/options", params={"refresh": "true" if refresh else "false"}),
        asyncio.to_thread(_load_codex_cache_metadata),
    )
    codex_by_slug = codex_cache["by_slug"]
    enriched_providers = []
    for prov in options.get("providers") or []:
        slug = str(prov.get("slug") or "")
        model_ids = [str(m) for m in (prov.get("models") or [])]
        model_details = [_build_model_detail(m, slug, codex_by_slug) for m in model_ids]
        enriched_providers.append(
            {
                **prov,
                "model_details": model_details,
            }
        )
    return {
        **options,
        "providers": enriched_providers,
        "codex_catalog": {
            "source": "codex-oauth-live-catalog",
            "fetched_at": codex_cache["fetched_at"],
            "client_version": codex_cache["client_version"],
            "cache_path": str(CODEX_MODELS_CACHE),
            "model_count": len(codex_by_slug),
        },
        "reasoning_efforts": ["none", *VALID_REASONING_EFFORTS],
    }


@app.post("/api/models/apply")
async def apply_model(body: ModelApplyRequest):
    if body.reasoning_effort and body.reasoning_effort not in ("none", *VALID_REASONING_EFFORTS):
        raise HTTPException(
            status_code=400,
            detail=f"Invalid reasoning_effort '{body.reasoning_effort}'. Allowed: none, {', '.join(VALID_REASONING_EFFORTS)}",
        )

    global_result = await _hermes_request(
        "POST",
        "/api/model/set",
        json_body={
            "scope": "main",
            "provider": body.provider,
            "model": body.model,
            "confirm_expensive_model": True,
        },
    )

    reasoning_result = None
    if body.reasoning_effort:
        reasoning_value: Any = False if body.reasoning_effort == "none" else body.reasoning_effort
        reasoning_result = await _hermes_request(
            "PUT",
            "/api/config",
            json_body={"config": {"agent": {"reasoning_effort": reasoning_value}}},
        )

    session_results: list[dict[str, Any]] = []
    if body.apply_to_active_sessions:
        sessions = await asyncio.to_thread(_load_sessions_snapshot, 20)
        active_targets = [s for s in sessions if s["is_active"]]
        for sess in active_targets:
            sid = sess["id"]
            try:
                res = await _mutate_session_via_gateway(
                    session_id=sid,
                    expected_revision=int(sess["runtime_revision"] or 0),
                    expected_generation=sess["runtime_generation"],
                    provider=body.provider,
                    model=body.model,
                    reasoning_effort=body.reasoning_effort,
                )
                session_results.append({"session_id": sid, "source": sess["source"], "ok": True, "result": res})
            except (GatewayClientError, HTTPException, Exception) as exc:
                detail = exc.detail if isinstance(exc, HTTPException) else str(exc)
                session_results.append({"session_id": sid, "source": sess["source"], "ok": False, "error": detail})

    await _broadcast_full_overview()
    return {
        "ok": True,
        "global_result": global_result,
        "reasoning_result": reasoning_result,
        "session_results": session_results,
    }


@app.get("/api/sessions")
async def list_sessions(limit: int = 30):
    sessions = await asyncio.to_thread(_load_sessions_snapshot, limit)
    return {"sessions": sessions}


@app.post("/api/sessions/new")
async def create_new_session(body: CreateSessionRequest):
    discovery = await asyncio.to_thread(discover_gateway_endpoint, HERMES_HOME, timeout=2.0)
    if discovery.endpoint is None:
        raise HTTPException(status_code=503, detail=f"Gateway endpoint unavailable: {discovery.state}")

    create_params: dict[str, Any] = {
        "request_id": uuid.uuid4().hex,
        "source": "gui",
        "cwd": str(HERMES_WORKSPACE),
    }
    if body.title and body.title.strip():
        create_params["title"] = body.title.strip()
    if body.model and body.model.strip():
        create_params["model"] = body.model.strip()
        if body.provider and body.provider.strip():
            create_params["provider"] = body.provider.strip()
        if body.reasoning_effort and body.reasoning_effort.strip():
            create_params["reasoning"] = body.reasoning_effort.strip().lower()
        create_params["follow_profile_config"] = False
    else:
        create_params["follow_profile_config"] = True

    try:
        async with connect_gateway(endpoint=discovery.endpoint) as client:
            res = await client.rpc("session.create", **create_params)
            sid = res.get("session_id") if isinstance(res, dict) else None
            sub_id = res.get("subscription_id") if isinstance(res, dict) else None
            if sid and sub_id:
                with contextlib.suppress(Exception):
                    await client.rpc("session.detach", session_id=sid, subscription_id=sub_id)
    except GatewayClientError as exc:
        raise HTTPException(status_code=502, detail=str(exc)) from exc

    await _broadcast_full_overview()
    return {"ok": True, "session_id": sid, "result": res}


@app.get("/api/sessions/{session_id}/messages")
async def get_session_messages(session_id: str, limit: int = 80):
    messages = await asyncio.to_thread(_load_session_messages_sync, session_id, limit)
    return {"session_id": session_id, "messages": messages}


@app.post("/api/sessions/{session_id}/send")
async def send_session_message(session_id: str, body: SendMessageRequest):
    text = body.text.strip()
    if not text:
        raise HTTPException(status_code=400, detail="Message text cannot be empty")
    if session_id in _active_turns:
        raise HTTPException(status_code=409, detail="Session already has an active turn running")

    entry: dict[str, Any] = {"client": None, "task": None}
    _active_turns[session_id] = entry
    task = asyncio.create_task(_run_streaming_turn(session_id, text))
    entry["task"] = task
    return {"ok": True, "session_id": session_id, "started": True}


@app.post("/api/sessions/{session_id}/interrupt")
async def interrupt_session_turn(session_id: str):
    entry = _active_turns.get(session_id)
    client = entry.get("client") if entry else None
    if client is not None:
        try:
            res = await client.rpc("session.interrupt", session_id=session_id)
            return {"ok": True, "session_id": session_id, "result": res}
        except Exception as exc:
            raise HTTPException(status_code=409, detail=str(exc)) from exc

    discovery = await asyncio.to_thread(discover_gateway_endpoint, HERMES_HOME, timeout=2.0)
    if discovery.endpoint is None:
        raise HTTPException(status_code=503, detail="Gateway unavailable")
    try:
        async with connect_gateway(endpoint=discovery.endpoint) as gw_client:
            await gw_client.rpc("session.resume", session_id=session_id, omit_messages=True)
            res = await gw_client.rpc("session.interrupt", session_id=session_id)
            return {"ok": True, "session_id": session_id, "result": res}
    except GatewayClientError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc


@app.delete("/api/sessions/{session_id}")
async def delete_session(session_id: str):
    def _get_row(conn: sqlite3.Connection) -> Optional[dict[str, Any]]:
        row = conn.execute(
            "SELECT id, runtime_revision, runtime_generation FROM sessions WHERE id = ?",
            (session_id,),
        ).fetchone()
        return dict(row) if row else None

    row = await asyncio.to_thread(_query_db, _get_row)
    if not row:
        raise HTTPException(status_code=404, detail=f"Session '{session_id}' not found")

    params: dict[str, Any] = {
        "request_id": uuid.uuid4().hex,
        "expected_revision": int(row["runtime_revision"] or 0),
    }
    if row["runtime_generation"] is not None:
        params["expected_generation"] = row["runtime_generation"]

    res = await _hermes_request("DELETE", f"/api/sessions/{session_id}", params=params)
    await _broadcast_full_overview()
    return {"ok": True, "session_id": session_id, "result": res}


@app.post("/api/sessions/{session_id}/model")
async def switch_session_model(session_id: str, body: SessionModelRequest):
    if body.reasoning_effort and body.reasoning_effort not in ("none", *VALID_REASONING_EFFORTS):
        raise HTTPException(
            status_code=400,
            detail=f"Invalid reasoning_effort '{body.reasoning_effort}'. Allowed: none, {', '.join(VALID_REASONING_EFFORTS)}",
        )

    def _get_row(conn: sqlite3.Connection) -> Optional[dict[str, Any]]:
        row = conn.execute(
            "SELECT id, source, runtime_revision, runtime_generation FROM sessions WHERE id = ?",
            (session_id,),
        ).fetchone()
        return dict(row) if row else None

    row = await asyncio.to_thread(_query_db, _get_row)
    if not row:
        raise HTTPException(status_code=404, detail=f"Session '{session_id}' not found")

    try:
        res = await _mutate_session_via_gateway(
            session_id=session_id,
            expected_revision=int(row["runtime_revision"] or 0),
            expected_generation=row["runtime_generation"],
            provider=body.provider,
            model=body.model,
            reasoning_effort=body.reasoning_effort,
        )
        await _broadcast_full_overview()
        return {"ok": True, "session_id": session_id, "result": res}
    except GatewayClientError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc


@app.get("/api/workspace/files")
async def list_workspace_files(root: str = "winapp", subpath: str = ""):
    base = WORKSPACE_ROOTS.get(root)
    if base is None:
        raise HTTPException(status_code=400, detail=f"Unknown workspace root '{root}'")
    target = (base / subpath).resolve() if subpath else base
    if target != base and base not in target.parents:
        raise HTTPException(status_code=403, detail="Path outside allowed workspace root")
    if not target.exists() or not target.is_dir():
        raise HTTPException(status_code=404, detail="Directory not found")

    def _scan() -> list[dict[str, Any]]:
        entries: list[dict[str, Any]] = []
        with os.scandir(target) as it:
            for item in it:
                if item.name.startswith(".") and item.name not in {".env", ".gitignore"}:
                    continue
                if item.name in {"node_modules", "__pycache__", ".git", "obj", "bin"}:
                    continue
                try:
                    stat = item.stat()
                    rel = Path(item.path).resolve().relative_to(base).as_posix()
                    entries.append(
                        {
                            "name": item.name,
                            "path": rel,
                            "is_dir": item.is_dir(),
                            "size": 0 if item.is_dir() else stat.st_size,
                            "mtime": stat.st_mtime,
                        }
                    )
                except OSError:
                    continue
        entries.sort(key=lambda e: (not e["is_dir"], e["name"].lower()))
        return entries[:200]

    entries = await asyncio.to_thread(_scan)
    rel_current = "" if target == base else target.relative_to(base).as_posix()
    return {"root": root, "base": str(base), "subpath": rel_current, "entries": entries}


@app.get("/api/workspace/file")
async def read_workspace_file(root: str = "winapp", path: str = ""):
    base = WORKSPACE_ROOTS.get(root)
    if base is None:
        raise HTTPException(status_code=400, detail=f"Unknown workspace root '{root}'")
    target = (base / path).resolve()
    if target != base and base not in target.parents:
        raise HTTPException(status_code=403, detail="Path outside allowed workspace root")
    if not target.exists() or not target.is_file():
        raise HTTPException(status_code=404, detail="File not found")

    def _read() -> tuple[str, int, bool]:
        raw = target.read_bytes()
        truncated = len(raw) > 65536
        text = raw[:65536].decode("utf-8", errors="replace")
        return text, len(raw), truncated

    content, size, truncated = await asyncio.to_thread(_read)
    return {
        "root": root,
        "path": target.relative_to(base).as_posix(),
        "size": size,
        "truncated": truncated,
        "content": content,
    }


@app.get("/api/mcp")
async def get_mcp_servers():
    data = await _hermes_request("GET", "/api/mcp/servers")
    return {
        **data,
        "node_mcp_entry": str(NODE_MCP_ENTRY),
        "node_mcp_built": NODE_MCP_ENTRY.exists(),
        "node_mcp_name": NODE_MCP_NAME,
    }


@app.post("/api/mcp/register-node")
async def register_node_mcp():
    if not NODE_MCP_ENTRY.exists():
        raise HTTPException(status_code=400, detail=f"NodeMcpServer build artifact missing: {NODE_MCP_ENTRY}")

    existing = await _hermes_request("GET", "/api/mcp/servers")
    servers = existing.get("servers", [])
    already = next((s for s in servers if s.get("name") == NODE_MCP_NAME), None)
    desired_args = [str(NODE_MCP_ENTRY)]

    if already:
        if already.get("command") == "node" and already.get("args") == desired_args and already.get("enabled", True):
            return {"ok": True, "already_registered": True, "server": already}
        await _hermes_request("DELETE", f"/api/mcp/servers/{NODE_MCP_NAME}")

    created = await _hermes_request(
        "POST",
        "/api/mcp/servers",
        json_body={
            "name": NODE_MCP_NAME,
            "command": "node",
            "args": desired_args,
        },
    )
    await _broadcast_full_overview()
    return {"ok": True, "already_registered": False, "server": created}


@app.post("/api/mcp/{name}/test")
async def test_mcp_server(name: str):
    return await _hermes_request("POST", f"/api/mcp/servers/{name}/test", timeout=35.0)


@app.post("/api/mcp/call-tool")
async def call_node_mcp_tool(body: McpToolCallRequest):
    if not NODE_MCP_ENTRY.exists():
        raise HTTPException(status_code=400, detail=f"NodeMcpServer build artifact missing: {NODE_MCP_ENTRY}")

    proc = await asyncio.create_subprocess_exec(
        "node",
        str(NODE_MCP_ENTRY),
        stdin=asyncio.subprocess.PIPE,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )
    requests = [
        {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "hermes-manager", "version": "2.0.0"},
            },
        },
        {"jsonrpc": "2.0", "method": "notifications/initialized"},
        {
            "jsonrpc": "2.0",
            "id": 2,
            "method": "tools/call",
            "params": {"name": body.tool_name, "arguments": body.arguments},
        },
    ]
    payload = ("\n".join(json.dumps(r) for r in requests) + "\n").encode("utf-8")
    try:
        stdout_bytes, stderr_bytes = await asyncio.wait_for(proc.communicate(payload), timeout=15.0)
    except asyncio.TimeoutError as exc:
        proc.kill()
        raise HTTPException(status_code=504, detail="NodeMcpServer tool call timed out") from exc

    stderr_text = stderr_bytes.decode("utf-8", errors="replace").strip()
    responses: list[dict[str, Any]] = []
    for line in stdout_bytes.decode("utf-8", errors="replace").splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            responses.append(json.loads(line))
        except json.JSONDecodeError:
            continue

    tool_reply = next((item for item in responses if item.get("id") == 2), None)
    if tool_reply is None:
        raise HTTPException(
            status_code=502,
            detail=f"No tool response from NodeMcpServer (stderr={stderr_text})",
        )

    result_obj = tool_reply.get("result") or tool_reply.get("error") or {}
    parsed_text: Any = None
    content = result_obj.get("content") if isinstance(result_obj, dict) else None
    if isinstance(content, list) and content and isinstance(content[0], dict):
        raw_text = content[0].get("text", "")
        try:
            parsed_text = json.loads(raw_text)
        except (ValueError, TypeError):
            parsed_text = raw_text

    return {
        "ok": not bool(result_obj.get("isError")) and "error" not in tool_reply,
        "tool_name": body.tool_name,
        "raw": result_obj,
        "parsed": parsed_text,
        "stderr": stderr_text,
    }


@app.post("/api/gateway/restart")
async def restart_gateway():
    return await _hermes_request("POST", "/api/gateway/restart")


async def _build_full_snapshot(refresh_models: bool = False) -> dict[str, Any]:
    overview, models, mcp = await asyncio.gather(
        get_overview(),
        get_models(refresh=refresh_models),
        get_mcp_servers(),
    )
    return {"overview": overview, "models": models, "mcp": mcp}


async def _broadcast_full_overview() -> None:
    try:
        snapshot = await _build_full_snapshot(refresh_models=False)
        await _ws_broadcast({"type": "snapshot", **snapshot})
    except Exception:
        pass


async def _handle_ws_action(action: str, payload: dict[str, Any]) -> Any:
    if action == "refresh":
        return await _build_full_snapshot(refresh_models=bool(payload.get("refresh_models")))
    if action == "apply_model":
        return await apply_model(ModelApplyRequest(**payload))
    if action == "switch_session_model":
        session_id = str(payload.get("session_id") or "")
        return await switch_session_model(session_id, SessionModelRequest(**payload))
    if action == "create_session":
        return await create_new_session(CreateSessionRequest(**payload))
    if action == "send_message":
        session_id = str(payload.get("session_id") or "")
        return await send_session_message(session_id, SendMessageRequest(**payload))
    if action == "interrupt_session":
        session_id = str(payload.get("session_id") or "")
        return await interrupt_session_turn(session_id)
    if action == "delete_session":
        session_id = str(payload.get("session_id") or "")
        return await delete_session(session_id)
    if action == "get_messages":
        session_id = str(payload.get("session_id") or "")
        limit = int(payload.get("limit") or 80)
        return await get_session_messages(session_id, limit=limit)
    if action == "register_node_mcp":
        return await register_node_mcp()
    if action == "test_mcp":
        name = str(payload.get("name") or NODE_MCP_NAME)
        return await test_mcp_server(name)
    if action == "call_tool":
        return await call_node_mcp_tool(McpToolCallRequest(**payload))
    if action == "restart_gateway":
        return await restart_gateway()
    raise HTTPException(status_code=400, detail=f"Unknown WebSocket action: {action}")


@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket):
    await ws.accept()
    async with _ws_clients_lock:
        _ws_clients.add(ws)
    try:
        try:
            snapshot = await _build_full_snapshot(refresh_models=False)
            await ws.send_json({"type": "snapshot", **snapshot})
        except Exception as exc:
            await ws.send_json({"type": "error", "error": str(exc)})

        while True:
            raw = await ws.receive_text()
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                await ws.send_json({"type": "response", "req_id": None, "ok": False, "error": "Invalid JSON"})
                continue

            req_id = msg.get("req_id")
            action = str(msg.get("action") or "")
            payload = msg.get("payload") if isinstance(msg.get("payload"), dict) else {}
            try:
                data = await _handle_ws_action(action, payload)
                await ws.send_json({"type": "response", "req_id": req_id, "ok": True, "data": data})
            except HTTPException as exc:
                await ws.send_json({"type": "response", "req_id": req_id, "ok": False, "error": exc.detail})
            except Exception as exc:
                await ws.send_json({"type": "response", "req_id": req_id, "ok": False, "error": str(exc)})
    except WebSocketDisconnect:
        pass
    finally:
        async with _ws_clients_lock:
            _ws_clients.discard(ws)


if __name__ == "__main__":
    host = os.environ.get("HERMES_MANAGER_HOST", "0.0.0.0")
    port = int(os.environ.get("HERMES_MANAGER_PORT", "45501"))
    uvicorn.run(app, host=host, port=port, log_level="info")
