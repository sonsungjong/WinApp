import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { ChatView } from "./components/ChatView";
import { McpToolsView } from "./components/McpToolsView";
import { ModelsCatalogView } from "./components/ModelsCatalogView";
import { WorkspaceView } from "./components/WorkspaceView";
import type {
  LiveTurnStream,
  McpListResponse,
  McpProbeResult,
  MessageItem,
  ModelDetail,
  ModelsResponse,
  OverviewResponse,
  SessionItem,
  WorkspaceEntry,
} from "./types";

type NavMode = "chat" | "models" | "mcp" | "files";
type SessionFilter = "all" | "telegram" | "local";
type InspectorTab = "session" | "mcp" | "files";
type WsStatus = "connecting" | "connected" | "disconnected";

interface PendingWsRequest {
  resolve: (data: unknown) => void;
  reject: (err: Error) => void;
  timer: number;
}

async function apiFetch<T>(url: string, init?: RequestInit): Promise<T> {
  const res = await fetch(url, init);
  const text = await res.text();
  let parsed: unknown = null;
  try {
    parsed = text ? JSON.parse(text) : null;
  } catch {
    parsed = text;
  }
  if (!res.ok) {
    const detail =
      typeof parsed === "object" && parsed !== null && "detail" in parsed
        ? JSON.stringify((parsed as { detail: unknown }).detail)
        : text || `HTTP ${res.status}`;
    throw new Error(detail);
  }
  return parsed as T;
}

function formatCompactTime(epoch: number | null): string {
  if (!epoch) return "-";
  return new Date(epoch * 1000).toLocaleTimeString("ko-KR", {
    hour: "2-digit",
    minute: "2-digit",
    hour12: false,
  });
}

export default function App() {
  const [navMode, setNavMode] = useState<NavMode>("chat");
  const [sidebarOpen, setSidebarOpen] = useState(true);
  const [inspectorOpen, setInspectorOpen] = useState(true);
  const [inspectorTab, setInspectorTab] = useState<InspectorTab>("session");

  const [overview, setOverview] = useState<OverviewResponse | null>(null);
  const [modelsData, setModelsData] = useState<ModelsResponse | null>(null);
  const [mcpData, setMcpData] = useState<McpListResponse | null>(null);

  const [activeSessionId, setActiveSessionId] = useState<string | null>(null);
  const [sessionFilter, setSessionFilter] = useState<SessionFilter>("all");
  const [sessionSearch, setSessionSearch] = useState("");
  const [messagesBySession, setMessagesBySession] = useState<Record<string, MessageItem[]>>({});
  const [liveStreams, setLiveStreams] = useState<Record<string, LiveTurnStream>>({});

  const [selectedProvider, setSelectedProvider] = useState("");
  const [selectedModel, setSelectedModel] = useState("");
  const [selectedEffort, setSelectedEffort] = useState("");
  const initializedModelRef = useRef(false);

  const [wsStatus, setWsStatus] = useState<WsStatus>("connecting");
  const wsRef = useRef<WebSocket | null>(null);
  const pendingRef = useRef<Map<string, PendingWsRequest>>(new Map());
  const reqSeqRef = useRef(0);

  const [busy, setBusy] = useState(false);
  const [toast, setToast] = useState<{ ok: boolean; text: string } | null>(null);

  const [probeResult, setProbeResult] = useState<McpProbeResult | null>(null);
  const [toolOutput, setToolOutput] = useState<unknown>(null);
  const [quickFiles, setQuickFiles] = useState<WorkspaceEntry[]>([]);

  const applyOverview = useCallback((ov: OverviewResponse) => {
    setOverview(ov);
    setActiveSessionId((prev) => {
      if (prev && ov.sessions.some((s) => s.id === prev)) return prev;
      return ov.telegram_session?.id ?? ov.sessions[0]?.id ?? null;
    });
    if (!initializedModelRef.current) {
      initializedModelRef.current = true;
      if (ov.global_model.provider) setSelectedProvider(ov.global_model.provider);
      if (ov.global_model.model) setSelectedModel(ov.global_model.model);
      if (ov.global_model.reasoning_effort) setSelectedEffort(ov.global_model.reasoning_effort);
    }
  }, []);

  const refreshViaRest = useCallback(async () => {
    const [ov, md, mcp] = await Promise.all([
      apiFetch<OverviewResponse>("/api/overview"),
      apiFetch<ModelsResponse>("/api/models"),
      apiFetch<McpListResponse>("/api/mcp"),
    ]);
    applyOverview(ov);
    setModelsData(md);
    setMcpData(mcp);
  }, [applyOverview]);

  useEffect(() => {
    let disposed = false;
    let reconnectTimer: number | null = null;

    const connectWs = () => {
      if (disposed) return;
      setWsStatus("connecting");
      const proto = window.location.protocol === "https:" ? "wss:" : "ws:";
      const ws = new WebSocket(`${proto}//${window.location.host}/ws`);
      wsRef.current = ws;

      ws.onopen = () => {
        if (!disposed) setWsStatus("connected");
      };

      ws.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data) as Record<string, unknown>;
          const msgType = msg.type;

          if (msgType === "snapshot") {
            if (msg.overview) applyOverview(msg.overview as OverviewResponse);
            if (msg.models) setModelsData(msg.models as ModelsResponse);
            if (msg.mcp) setMcpData(msg.mcp as McpListResponse);
            return;
          }
          if (msgType === "overview" && msg.overview) {
            applyOverview(msg.overview as OverviewResponse);
            return;
          }
          if (msgType === "messages_update" && typeof msg.session_id === "string" && Array.isArray(msg.messages)) {
            const sid = msg.session_id;
            setMessagesBySession((prev) => ({ ...prev, [sid]: msg.messages as MessageItem[] }));
            setLiveStreams((prev) => {
              const next = { ...prev };
              delete next[sid];
              return next;
            });
            return;
          }
          if (msgType === "chat_event" && typeof msg.session_id === "string") {
            const sid = msg.session_id;
            const kind = String(msg.event_type || "");
            const payload = (msg.payload as Record<string, unknown>) || {};
            setLiveStreams((prev) => {
              const current: LiveTurnStream = prev[sid] ?? {
                thinking: "",
                reasoning: "",
                text: "",
                tools: [],
              };
              if (kind === "turn.started" || kind === "message.start") {
                if (prev[sid]) {
                  return prev;
                }
                return { ...prev, [sid]: { thinking: "응답 준비 중...", reasoning: "", text: "", tools: [] } };
              }
              if (kind === "thinking.delta") {
                return { ...prev, [sid]: { ...current, thinking: String(payload.text || "") } };
              }
              if (kind === "agent.step") {
                const step = payload.iteration ? ` (step ${String(payload.iteration)})` : "";
                return { ...prev, [sid]: { ...current, thinking: `에이전트 추론 진행 중...${step}` } };
              }
              if (kind === "status.update") {
                return { ...prev, [sid]: { ...current, thinking: String(payload.text || "처리 중...") } };
              }
              if (kind === "reasoning.delta") {
                return {
                  ...prev,
                  [sid]: { ...current, reasoning: current.reasoning + String(payload.text || "") },
                };
              }
              if (kind === "message.delta") {
                const delta = payload.text;
                return {
                  ...prev,
                  [sid]: {
                    ...current,
                    text: delta === null ? `${current.text}\n` : current.text + String(delta || ""),
                  },
                };
              }
              if (kind === "message.interim") {
                const text = String(payload.text || "");
                return {
                  ...prev,
                  [sid]: { ...current, text: current.text ? `${current.text}\n${text}` : text },
                };
              }
              if (kind === "tool.generating") {
                return { ...prev, [sid]: { ...current, thinking: `도구 준비 중: ${String(payload.name || "")}` } };
              }
              if (kind === "tool.start") {
                const toolName = String(payload.name || payload.tool_name || "tool");
                const detail = String(payload.context || "");
                return {
                  ...prev,
                  [sid]: {
                    ...current,
                    thinking: `도구 실행 중: ${toolName}`,
                    tools: [...current.tools, { name: toolName, status: "running", detail: detail || undefined }],
                  },
                };
              }
              if (kind === "tool.complete") {
                const toolName = String(payload.name || payload.tool_name || "tool");
                let flipped = false;
                const tools = current.tools.map((t) => {
                  if (!flipped && t.name === toolName && t.status === "running") {
                    flipped = true;
                    return { ...t, status: "done" as const };
                  }
                  return t;
                });
                return { ...prev, [sid]: { ...current, tools } };
              }
              if (kind === "turn.error") {
                setToast({ ok: false, text: `응답 오류: ${String(payload.error || "unknown error")}` });
                const next = { ...prev };
                delete next[sid];
                return next;
              }
              if (kind === "message.complete" || kind === "turn.cancelled") {
                const next = { ...prev };
                delete next[sid];
                return next;
              }
              return prev;
            });
            return;
          }
          if (msgType === "response" && typeof msg.req_id === "string") {
            const pending = pendingRef.current.get(msg.req_id);
            if (pending) {
              window.clearTimeout(pending.timer);
              pendingRef.current.delete(msg.req_id);
              if (msg.ok) {
                pending.resolve(msg.data);
              } else {
                const errText =
                  typeof msg.error === "string" ? msg.error : JSON.stringify(msg.error ?? "WebSocket error");
                pending.reject(new Error(errText));
              }
            }
          }
        } catch {
          // Ignore malformed WS frames
        }
      };

      ws.onclose = () => {
        if (wsRef.current === ws) wsRef.current = null;
        for (const [id, pending] of pendingRef.current.entries()) {
          window.clearTimeout(pending.timer);
          pending.reject(new Error("WebSocket disconnected"));
          pendingRef.current.delete(id);
        }
        if (!disposed) {
          setWsStatus("disconnected");
          reconnectTimer = window.setTimeout(connectWs, 2000);
        }
      };

      ws.onerror = () => ws.close();
    };

    connectWs();
    refreshViaRest().catch(() => {});

    return () => {
      disposed = true;
      if (reconnectTimer !== null) window.clearTimeout(reconnectTimer);
      wsRef.current?.close();
      wsRef.current = null;
    };
  }, [applyOverview, refreshViaRest]);

  const sendAction = useCallback(
    async <T,>(action: string, payload: Record<string, unknown>, restFallback: () => Promise<T>): Promise<T> => {
      const ws = wsRef.current;
      if (ws && ws.readyState === WebSocket.OPEN) {
        const reqId = `req-${++reqSeqRef.current}`;
        return new Promise<T>((resolve, reject) => {
          const timer = window.setTimeout(() => {
            pendingRef.current.delete(reqId);
            reject(new Error(`WebSocket timeout on ${action}`));
          }, 40000);
          pendingRef.current.set(reqId, {
            resolve: (data) => resolve(data as T),
            reject,
            timer,
          });
          ws.send(JSON.stringify({ req_id: reqId, action, payload }));
        });
      }
      return restFallback();
    },
    []
  );

  const activeSession: SessionItem | null = useMemo(() => {
    if (!overview?.sessions.length) return null;
    return overview.sessions.find((s) => s.id === activeSessionId) ?? overview.sessions[0] ?? null;
  }, [overview, activeSessionId]);

  useEffect(() => {
    if (!activeSessionId) return;
    sendAction<{ messages: MessageItem[] }>("get_messages", { session_id: activeSessionId, limit: 300 }, () =>
      apiFetch(`/api/sessions/${encodeURIComponent(activeSessionId)}/messages?limit=300`)
    )
      .then((res) => {
        setMessagesBySession((prev) => ({ ...prev, [activeSessionId]: res.messages }));
      })
      .catch(() => {});
  }, [activeSessionId, activeSession?.message_count, activeSession?.last_activity_at, sendAction]);

  useEffect(() => {
    if (inspectorTab === "files" && quickFiles.length === 0) {
      apiFetch<{ entries: WorkspaceEntry[] }>("/api/workspace/files?root=winapp")
        .then((res) => setQuickFiles(res.entries))
        .catch(() => {});
    }
  }, [inspectorTab, quickFiles.length]);

  useEffect(() => {
    if (!activeSession) return;
    if (activeSession.effective_provider) setSelectedProvider(activeSession.effective_provider);
    if (activeSession.effective_model) setSelectedModel(activeSession.effective_model);
  }, [activeSession?.id]);

  const providers = modelsData?.providers ?? [];

  const selectedModelDetail: ModelDetail | null = useMemo(() => {
    const prov = providers.find((p) => p.slug === selectedProvider) ?? providers[0];
    return prov?.model_details?.find((m) => m.id === selectedModel) ?? null;
  }, [providers, selectedProvider, selectedModel]);

  const availableEfforts = useMemo(() => {
    if (selectedModelDetail?.supported_reasoning_levels?.length) {
      return selectedModelDetail.supported_reasoning_levels;
    }
    return (modelsData?.reasoning_efforts ?? ["low", "medium", "high", "xhigh", "max", "ultra"]).map((effort) => ({
      effort,
      description: "",
    }));
  }, [selectedModelDetail, modelsData]);

  useEffect(() => {
    if (availableEfforts.length === 0) return;
    if (!availableEfforts.some((e) => e.effort === selectedEffort)) {
      setSelectedEffort(selectedModelDetail?.default_reasoning_level || availableEfforts[0].effort);
    }
  }, [availableEfforts, selectedEffort, selectedModelDetail]);

  const filteredSessions = useMemo(() => {
    const list = overview?.sessions ?? [];
    return list.filter((s) => {
      if (sessionFilter === "telegram" && s.source !== "telegram") return false;
      if (sessionFilter === "local" && s.source === "telegram") return false;
      if (sessionSearch.trim()) {
        const q = sessionSearch.toLowerCase();
        const hay = `${s.title ?? ""} ${s.display_name ?? ""} ${s.id} ${s.effective_model}`.toLowerCase();
        if (!hay.includes(q)) return false;
      }
      return true;
    });
  }, [overview, sessionFilter, sessionSearch]);

  const handleCreateSession = async () => {
    setBusy(true);
    setToast(null);
    try {
      const res = await sendAction<{ session_id: string }>(
        "create_session",
        {
          provider: selectedProvider || undefined,
          model: selectedModel || undefined,
          reasoning_effort: selectedEffort || undefined,
        },
        () =>
          apiFetch("/api/sessions/new", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              provider: selectedProvider || null,
              model: selectedModel || null,
              reasoning_effort: selectedEffort || null,
            }),
          })
      );
      if (res.session_id) {
        setActiveSessionId(res.session_id);
        setNavMode("chat");
      }
      setToast({ ok: true, text: `새 대화 세션 생성 완료 (${selectedModel || "기본 모델"})` });
    } catch (err) {
      setToast({ ok: false, text: `새 대화 생성 실패: ${(err as Error).message}` });
    } finally {
      setBusy(false);
    }
  };

  const handleDeleteSession = async (sessionId: string) => {
    setBusy(true);
    try {
      await sendAction("delete_session", { session_id: sessionId }, () =>
        apiFetch(`/api/sessions/${encodeURIComponent(sessionId)}`, { method: "DELETE" })
      );
      if (activeSessionId === sessionId) setActiveSessionId(null);
      setToast({ ok: true, text: `세션 ${sessionId.slice(0, 14)} 삭제 완료` });
    } catch (err) {
      setToast({ ok: false, text: `세션 삭제 실패: ${(err as Error).message}` });
    } finally {
      setBusy(false);
    }
  };

  const handleApplyGlobalAndActive = async (prov = selectedProvider, mod = selectedModel) => {
    setBusy(true);
    setToast(null);
    const body = {
      provider: prov,
      model: mod,
      reasoning_effort: selectedEffort || null,
      apply_to_active_sessions: true,
    };
    try {
      const res = await sendAction<{
        session_results: Array<{ source: string; ok: boolean; error?: string }>;
      }>("apply_model", body, () =>
        apiFetch("/api/models/apply", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(body),
        })
      );
      setSelectedProvider(prov);
      setSelectedModel(mod);
      const summary = res.session_results.map((r) => `${r.source}:${r.ok ? "OK" : "ERR"}`).join(", ");
      setToast({
        ok: true,
        text: `전역 + 활성 세션 모델 → ${mod} (${selectedEffort}) 적용 완료${summary ? ` [${summary}]` : ""}`,
      });
    } catch (err) {
      setToast({ ok: false, text: `모델 적용 실패: ${(err as Error).message}` });
    } finally {
      setBusy(false);
    }
  };

  const handleApplyCurrentSessionOnly = async () => {
    if (!activeSession) return;
    setBusy(true);
    setToast(null);
    const body = {
      session_id: activeSession.id,
      provider: selectedProvider,
      model: selectedModel,
      reasoning_effort: selectedEffort || null,
    };
    try {
      await sendAction("switch_session_model", body, () =>
        apiFetch(`/api/sessions/${encodeURIComponent(activeSession.id)}/model`, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            provider: selectedProvider,
            model: selectedModel,
            reasoning_effort: selectedEffort || null,
          }),
        })
      );
      setToast({
        ok: true,
        text: `[${activeSession.source}] 세션 모델 → ${selectedModel} (${selectedEffort}) 변경 완료`,
      });
    } catch (err) {
      setToast({ ok: false, text: `세션 모델 변경 실패: ${(err as Error).message}` });
    } finally {
      setBusy(false);
    }
  };

  const handleSendMessage = async (text: string) => {
    if (!activeSession) return;
    const sid = activeSession.id;
    setMessagesBySession((prev) => ({
      ...prev,
      [sid]: [
        ...(prev[sid] ?? []),
        {
          id: Date.now(),
          role: "user",
          content: text,
          tool_name: null,
          tool_calls: [],
          tool_call_id: null,
          timestamp: Date.now() / 1000,
        },
      ],
    }));
    try {
      await sendAction("send_message", { session_id: sid, text }, () =>
        apiFetch(`/api/sessions/${encodeURIComponent(sid)}/send`, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ text }),
        })
      );
    } catch (err) {
      setToast({ ok: false, text: `메시지 전송 실패: ${(err as Error).message}` });
    }
  };

  const handleInterrupt = async () => {
    if (!activeSession) return;
    try {
      await sendAction("interrupt_session", { session_id: activeSession.id }, () =>
        apiFetch(`/api/sessions/${encodeURIComponent(activeSession.id)}/interrupt`, { method: "POST" })
      );
      setToast({ ok: true, text: "실행 중인 턴 중단을 요청했습니다." });
    } catch (err) {
      setToast({ ok: false, text: `중단 실패: ${(err as Error).message}` });
    }
  };

  const handleRefreshCatalog = async () => {
    setBusy(true);
    try {
      const md = await apiFetch<ModelsResponse>("/api/models?refresh=true");
      setModelsData(md);
      setToast({ ok: true, text: "Codex 및 Hermes 모델 카탈로그를 새로고침했습니다." });
    } catch (err) {
      setToast({ ok: false, text: `카탈로그 새로고침 실패: ${(err as Error).message}` });
    } finally {
      setBusy(false);
    }
  };

  const handleRegisterNodeMcp = async () => {
    setBusy(true);
    try {
      const res = await sendAction<{ already_registered: boolean }>("register_node_mcp", {}, () =>
        apiFetch("/api/mcp/register-node", { method: "POST" })
      );
      setToast({
        ok: true,
        text: res.already_registered ? "node_winapp_mcp 이미 등록됨" : "node_winapp_mcp 등록 완료",
      });
    } catch (err) {
      setToast({ ok: false, text: `MCP 등록 실패: ${(err as Error).message}` });
    } finally {
      setBusy(false);
    }
  };

  const handleProbeMcp = async (name: string) => {
    setBusy(true);
    try {
      const res = await sendAction<McpProbeResult>("test_mcp", { name }, () =>
        apiFetch(`/api/mcp/${encodeURIComponent(name)}/test`, { method: "POST" })
      );
      setProbeResult(res);
      setToast({
        ok: res.ok,
        text: res.ok ? `Probe 성공 (${res.tools.length}개 도구)` : `Probe 실패: ${res.error ?? ""}`,
      });
    } catch (err) {
      setToast({ ok: false, text: `Probe 오류: ${(err as Error).message}` });
    } finally {
      setBusy(false);
    }
  };

  const handleCallTool = async (toolName: string, args: Record<string, unknown>) => {
    setBusy(true);
    try {
      const res = await sendAction<{ ok: boolean; parsed: unknown; raw: unknown }>(
        "call_tool",
        { tool_name: toolName, arguments: args },
        () =>
          apiFetch("/api/mcp/call-tool", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ tool_name: toolName, arguments: args }),
          })
      );
      setToolOutput(res.parsed ?? res.raw);
      setToast({ ok: res.ok, text: `MCP [${toolName}] 실행 완료` });
    } catch (err) {
      setToast({ ok: false, text: `도구 실행 오류: ${(err as Error).message}` });
    } finally {
      setBusy(false);
    }
  };

  const handleRestartGateway = async () => {
    setBusy(true);
    try {
      await sendAction("restart_gateway", {}, () => apiFetch("/api/gateway/restart", { method: "POST" }));
      setToast({ ok: true, text: "Hermes 게이트웨이 재시작 요청 전송 완료" });
    } catch (err) {
      setToast({ ok: false, text: `재시작 실패: ${(err as Error).message}` });
    } finally {
      setBusy(false);
    }
  };

  const telegramState = overview?.gateway.platforms.telegram?.state ?? "unknown";

  return (
    <div className="studio-root">
      {sidebarOpen && (
        <aside className="studio-sidebar">
          <div className="sidebar-brand">
            <div>
              <div className="brand-title">☤ Hermes Studio</div>
              <div className="brand-status">
                <span className={`dot ${wsStatus === "connected" ? "dot-ok" : "dot-warn"}`} />
                <span>WS</span>
                <span className={`dot ${overview?.gateway.running ? "dot-ok" : "dot-err"}`} />
                <span>GW</span>
                <span className={`dot ${telegramState === "connected" ? "dot-ok" : "dot-err"}`} />
                <span>TG</span>
              </div>
            </div>
            <button className="btn-icon" onClick={handleRestartGateway} title="게이트웨이 재시작" type="button">
              ↻
            </button>
          </div>

          <nav className="sidebar-nav">
            <button
              className={`nav-item ${navMode === "chat" ? "nav-item-active" : ""}`}
              onClick={() => setNavMode("chat")}
              type="button"
            >
              💬 Chat Studio
            </button>
            <button
              className={`nav-item ${navMode === "models" ? "nav-item-active" : ""}`}
              onClick={() => setNavMode("models")}
              type="button"
            >
              📊 Models & Codex
            </button>
            <button
              className={`nav-item ${navMode === "mcp" ? "nav-item-active" : ""}`}
              onClick={() => setNavMode("mcp")}
              type="button"
            >
              🔌 MCP & Tools ({mcpData?.servers.length ?? 0})
            </button>
            <button
              className={`nav-item ${navMode === "files" ? "nav-item-active" : ""}`}
              onClick={() => setNavMode("files")}
              type="button"
            >
              📁 Workspace Files
            </button>
          </nav>

          <div className="sidebar-sessions-head">
            <button className="btn-primary btn-full" onClick={handleCreateSession} disabled={busy} type="button">
              + 새 대화 시작
            </button>
            <div className="filter-pills">
              {(["all", "telegram", "local"] as SessionFilter[]).map((f) => (
                <button
                  key={f}
                  type="button"
                  className={`filter-pill ${sessionFilter === f ? "filter-pill-active" : ""}`}
                  onClick={() => setSessionFilter(f)}
                >
                  {f === "all" ? "전체" : f === "telegram" ? "Telegram" : "Local"}
                </button>
              ))}
            </div>
            <input
              type="text"
              className="sidebar-search"
              placeholder="세션 / 모델 검색..."
              value={sessionSearch}
              onChange={(e) => setSessionSearch(e.target.value)}
            />
          </div>

          <div className="sidebar-sessions-list">
            {filteredSessions.map((sess) => {
              const isSelected = activeSession?.id === sess.id && navMode === "chat";
              return (
                <div
                  key={sess.id}
                  className={`session-row ${isSelected ? "session-row-active" : ""}`}
                  onClick={() => {
                    setActiveSessionId(sess.id);
                    setNavMode("chat");
                  }}
                >
                  <div className="session-row-top">
                    <span className={`source-pill source-${sess.source}`}>
                      {sess.source === "telegram" ? "TG" : sess.source.toUpperCase()}
                    </span>
                    <span className="session-row-title">{sess.title || sess.display_name || sess.id}</span>
                    {sess.is_streaming && <span className="streaming-pill">응답중</span>}
                    {sess.source !== "telegram" && (
                      <button
                        type="button"
                        className="session-del-btn"
                        title="세션 삭제"
                        onClick={(e) => {
                          e.stopPropagation();
                          handleDeleteSession(sess.id);
                        }}
                      >
                        ×
                      </button>
                    )}
                  </div>
                  <div className="session-row-meta">
                    <span className="mono">{sess.effective_model || "-"}</span>
                    <span>
                      {sess.is_active && <span className="dot dot-ok" style={{ marginRight: 4 }} />}
                      {formatCompactTime(sess.last_activity_at)}
                    </span>
                  </div>
                </div>
              );
            })}
          </div>
        </aside>
      )}

      <main className="studio-main">
        <div className="mobile-bar">
          <button className="btn-ghost btn-xs" type="button" onClick={() => setSidebarOpen((v) => !v)}>
            {sidebarOpen ? "◀ 사이드바 접기" : "▶ 사이드바 열기"}
          </button>
          <span className="muted-text mono">
            {overview?.global_model.model} · TG:{overview?.telegram_session?.effective_model ?? "-"}
          </span>
        </div>

        {toast && (
          <div className={`studio-toast ${toast.ok ? "toast-ok" : "toast-err"}`}>
            <span>{toast.text}</span>
            <button type="button" className="toast-close" onClick={() => setToast(null)}>
              ×
            </button>
          </div>
        )}

        {navMode === "chat" && (
          <ChatView
            session={activeSession}
            overview={overview}
            messages={activeSession ? messagesBySession[activeSession.id] ?? [] : []}
            liveStream={activeSession ? liveStreams[activeSession.id] ?? null : null}
            providers={providers}
            selectedProvider={selectedProvider}
            selectedModel={selectedModel}
            selectedEffort={selectedEffort}
            selectedModelDetail={selectedModelDetail}
            availableEfforts={availableEfforts}
            busy={busy}
            inspectorOpen={inspectorOpen}
            onToggleInspector={() => setInspectorOpen((v) => !v)}
            onChangeProvider={(v) => setSelectedProvider(v)}
            onChangeModel={(v) => setSelectedModel(v)}
            onChangeEffort={(v) => setSelectedEffort(v)}
            onApplySessionModel={handleApplyCurrentSessionOnly}
            onApplyGlobalModel={() => handleApplyGlobalAndActive()}
            onSendMessage={handleSendMessage}
            onInterrupt={handleInterrupt}
          />
        )}

        {navMode === "models" && (
          <ModelsCatalogView
            overview={overview}
            modelsData={modelsData}
            selectedEffort={selectedEffort}
            busy={busy}
            onChangeEffort={setSelectedEffort}
            onRefreshCatalog={handleRefreshCatalog}
            onApplyModel={(prov, mod) => handleApplyGlobalAndActive(prov, mod)}
          />
        )}

        {navMode === "mcp" && (
          <McpToolsView
            mcpData={mcpData}
            probeResult={probeResult}
            toolOutput={toolOutput}
            busy={busy}
            onRegisterNodeMcp={handleRegisterNodeMcp}
            onProbeMcp={handleProbeMcp}
            onCallTool={handleCallTool}
          />
        )}

        {navMode === "files" && <WorkspaceView apiFetch={apiFetch} />}
      </main>

      {navMode === "chat" && inspectorOpen && (
        <aside className="studio-inspector">
          <div className="inspector-tabs">
            <button
              type="button"
              className={`inspector-tab ${inspectorTab === "session" ? "inspector-tab-active" : ""}`}
              onClick={() => setInspectorTab("session")}
            >
              세션 정보
            </button>
            <button
              type="button"
              className={`inspector-tab ${inspectorTab === "mcp" ? "inspector-tab-active" : ""}`}
              onClick={() => setInspectorTab("mcp")}
            >
              MCP 실행
            </button>
            <button
              type="button"
              className={`inspector-tab ${inspectorTab === "files" ? "inspector-tab-active" : ""}`}
              onClick={() => setInspectorTab("files")}
            >
              파일
            </button>
          </div>

          <div className="inspector-body">
            {inspectorTab === "session" && activeSession && (
              <div className="inspector-stack">
                <div className="kv-card">
                  <div className="kv-label">세션 실제 모델</div>
                  <div className="kv-val mono">{activeSession.effective_model || "-"}</div>
                  <div className="kv-sub">
                    {activeSession.has_override ? "세션 오버라이드 적용됨" : "기본 설정 상속"}
                  </div>
                </div>

                <div className="kv-card">
                  <div className="kv-label">전역 기본 모델 (config.yaml)</div>
                  <div className="kv-val mono">{overview?.global_model.model || "-"}</div>
                  <div className="kv-sub">
                    effort: {overview?.global_model.reasoning_effort || "-"} ·{" "}
                    {overview?.global_model.provider}
                  </div>
                </div>

                <div className="kv-card">
                  <div className="kv-label">선택된 모델 스펙 (Codex)</div>
                  <div className="kv-val">{selectedModelDetail?.display_name || selectedModel}</div>
                  <div className="kv-sub">{selectedModelDetail?.description || "-"}</div>
                </div>

                <div className="kv-card">
                  <div className="kv-label">토큰 사용량</div>
                  <div className="kv-grid">
                    <span>Input: {activeSession.input_tokens.toLocaleString()}</span>
                    <span>Output: {activeSession.output_tokens.toLocaleString()}</span>
                    <span>Cache: {activeSession.cache_read_tokens.toLocaleString()}</span>
                    <span>Reasoning: {activeSession.reasoning_tokens.toLocaleString()}</span>
                  </div>
                </div>

                <button
                  type="button"
                  className="btn-primary btn-full"
                  disabled={busy}
                  onClick={() => handleApplyGlobalAndActive()}
                >
                  전역 + 텔레그램 세션 강제 동기화
                </button>
              </div>
            )}

            {inspectorTab === "mcp" && (
              <div className="inspector-stack">
                <div className="row-gap">
                  <button className="btn-primary btn-xs" type="button" onClick={handleRegisterNodeMcp} disabled={busy}>
                    NodeMcp 등록
                  </button>
                  <button
                    className="btn-secondary btn-xs"
                    type="button"
                    onClick={() => handleProbeMcp(mcpData?.node_mcp_name || "node_winapp_mcp")}
                    disabled={busy}
                  >
                    Probe 테스트
                  </button>
                </div>
                <button
                  className="btn-secondary btn-sm btn-full"
                  type="button"
                  disabled={busy}
                  onClick={() => handleCallTool("get_system_status", {})}
                >
                  실행: get_system_status
                </button>
                <button
                  className="btn-secondary btn-sm btn-full"
                  type="button"
                  disabled={busy}
                  onClick={() => handleCallTool("list_winapp_projects", {})}
                >
                  실행: list_winapp_projects
                </button>
                <button
                  className="btn-secondary btn-sm btn-full"
                  type="button"
                  disabled={busy}
                  onClick={() => handleCallTool("generate_random_number", { min: 1, max: 100, count: 3 })}
                >
                  실행: generate_random_number (3개)
                </button>
                {toolOutput !== null && (
                  <pre className="json-preview">{JSON.stringify(toolOutput, null, 2)}</pre>
                )}
              </div>
            )}

            {inspectorTab === "files" && (
              <div className="inspector-stack">
                <div className="kv-sub">C:\Users\cbmplus\source\WinApp</div>
                {quickFiles.map((f) => (
                  <div key={f.path} className="quick-file-row">
                    <span>
                      {f.is_dir ? "📁" : "📄"} {f.name}
                    </span>
                    <button
                      type="button"
                      className="btn-ghost btn-xs"
                      onClick={() => setNavMode("files")}
                    >
                      열기
                    </button>
                  </div>
                ))}
              </div>
            )}
          </div>
        </aside>
      )}
    </div>
  );
}
