import { useEffect, useRef, useState } from "react";
import type {
  LiveTurnStream,
  MessageItem,
  ModelDetail,
  OverviewResponse,
  ProviderOption,
  SessionItem,
} from "../types";

interface ChatViewProps {
  session: SessionItem | null;
  overview: OverviewResponse | null;
  messages: MessageItem[];
  liveStream: LiveTurnStream | null;
  providers: ProviderOption[];
  selectedProvider: string;
  selectedModel: string;
  selectedEffort: string;
  selectedModelDetail: ModelDetail | null;
  availableEfforts: Array<{ effort: string; description: string }>;
  busy: boolean;
  inspectorOpen: boolean;
  onToggleInspector: () => void;
  onChangeProvider: (v: string) => void;
  onChangeModel: (v: string) => void;
  onChangeEffort: (v: string) => void;
  onApplySessionModel: () => void;
  onApplyGlobalModel: () => void;
  onSendMessage: (text: string) => void;
  onInterrupt: () => void;
}

function formatMessageTime(ts: number | null): string {
  if (!ts) return "";
  return new Date(ts * 1000).toLocaleTimeString("ko-KR", {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    hour12: false,
  });
}

function formatCtx(ctx?: number | null): string {
  if (!ctx) return "";
  if (ctx >= 1000) return `${Math.round(ctx / 1000)}K`;
  return String(ctx);
}

function renderFormattedText(text: string) {
  const parts = text.split(/(```[\s\S]*?```)/g);
  return parts.map((part, idx) => {
    if (part.startsWith("```") && part.endsWith("```")) {
      const inner = part.slice(3, -3).replace(/^[a-zA-Z0-9_-]+\n/, "");
      return (
        <pre key={idx} className="chat-code-block">
          <code>{inner}</code>
        </pre>
      );
    }
    return (
      <span key={idx} className="chat-plain-text">
        {part}
      </span>
    );
  });
}

export function ChatView(props: ChatViewProps) {
  const {
    session,
    overview,
    messages,
    liveStream,
    providers,
    selectedProvider,
    selectedModel,
    selectedEffort,
    selectedModelDetail,
    availableEfforts,
    busy,
    inspectorOpen,
    onToggleInspector,
    onChangeProvider,
    onChangeModel,
    onChangeEffort,
    onApplySessionModel,
    onApplyGlobalModel,
    onSendMessage,
    onInterrupt,
  } = props;

  const [draft, setDraft] = useState("");
  const [expandedTools, setExpandedTools] = useState<Record<number, boolean>>({});
  const scrollRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [messages.length, liveStream?.text, liveStream?.tools.length, session?.id]);

  const currentProvider = providers.find((p) => p.slug === selectedProvider) ?? providers[0];
  const modelDetails = currentProvider?.model_details ?? [];

  const handleSubmit = () => {
    const trimmed = draft.trim();
    if (!trimmed || !session) return;
    setDraft("");
    onSendMessage(trimmed);
  };

  if (!session) {
    return (
      <div className="empty-stage">
        <div className="empty-card">
          <h2>세션을 선택하거나 새 대화를 시작하세요</h2>
          <p>
            좌측 사이드바에서 <strong>Telegram</strong> 세션이나 <strong>Local</strong> 세션을 선택하면
            전체 대화 내역과 도구 호출 로그를 확인하고 바로 메시지를 보낼 수 있습니다.
          </p>
        </div>
      </div>
    );
  }

  const isStreaming = Boolean(session.is_streaming || liveStream);

  return (
    <div className="chat-shell">
      <div className="chat-topbar">
        <div className="chat-topbar-left">
          <span className={`source-pill source-${session.source}`}>
            {session.source.toUpperCase()}
          </span>
          <div className="chat-title-group">
            <div className="chat-title">{session.title || session.display_name || session.id}</div>
            <div className="chat-subtitle mono">
              {session.id} · active: <strong>{session.effective_model || "-"}</strong> · global:{" "}
              {overview?.global_model.model || "-"} · rev {session.runtime_revision}
            </div>
          </div>
        </div>
        <div className="chat-topbar-actions">
          <button className="btn-ghost" onClick={onToggleInspector} type="button">
            {inspectorOpen ? "인스펙터 숨기기" : "인스펙터 열기"}
          </button>
        </div>
      </div>

      <div className="chat-messages" ref={scrollRef}>
        {messages.length === 0 && !liveStream && (
          <div className="chat-empty-hint">
            아직 이 세션에 메시지가 없습니다. 아래 입력창에서 첫 메시지를 보내 보세요.
          </div>
        )}

        {messages.map((msg) => {
          if (msg.role === "tool") {
            const isOpen = Boolean(expandedTools[msg.id]);
            return (
              <div key={msg.id} className="tool-card">
                <button
                  type="button"
                  className="tool-card-header"
                  onClick={() =>
                    setExpandedTools((prev) => ({ ...prev, [msg.id]: !prev[msg.id] }))
                  }
                >
                  <span>🔧 Tool Result: {msg.tool_name || "tool"}</span>
                  <span className="tool-card-meta">
                    {formatMessageTime(msg.timestamp)} · {isOpen ? "접기 ▲" : "펼치기 ▼"}
                  </span>
                </button>
                {isOpen && <pre className="tool-card-body">{msg.content}</pre>}
              </div>
            );
          }

          if (msg.role === "assistant" && !msg.content && msg.tool_calls.length > 0) {
            const isOpen = Boolean(expandedTools[msg.id]);
            return (
              <div key={msg.id} className="tool-card tool-call-invoke">
                <button
                  type="button"
                  className="tool-card-header"
                  onClick={() =>
                    setExpandedTools((prev) => ({ ...prev, [msg.id]: !prev[msg.id] }))
                  }
                >
                  <span>
                    ⚡ Tool Call: {msg.tool_calls.map((c) => c.name).join(", ")}
                  </span>
                  <span className="tool-card-meta">
                    {formatMessageTime(msg.timestamp)} · {isOpen ? "접기 ▲" : "인자 보기 ▼"}
                  </span>
                </button>
                {isOpen && (
                  <pre className="tool-card-body">
                    {JSON.stringify(msg.tool_calls, null, 2)}
                  </pre>
                )}
              </div>
            );
          }

          const isUser = msg.role === "user";
          return (
            <div key={msg.id} className={`bubble-row ${isUser ? "bubble-row-user" : "bubble-row-assistant"}`}>
              <div className={`bubble ${isUser ? "bubble-user" : "bubble-assistant"}`}>
                <div className="bubble-meta">
                  <span>{isUser ? "You" : "Hermes"}</span>
                  <span>{formatMessageTime(msg.timestamp)}</span>
                </div>
                {msg.reasoning && (
                  <details className="reasoning-details">
                    <summary>추론 과정 보기</summary>
                    <pre className="reasoning-pre">{msg.reasoning}</pre>
                  </details>
                )}
                <div className="bubble-content">{renderFormattedText(msg.content)}</div>
                {msg.tool_calls.length > 0 && (
                  <div className="inline-tools-note">
                    호출된 도구: {msg.tool_calls.map((t) => t.name).join(", ")}
                  </div>
                )}
              </div>
            </div>
          );
        })}

        {liveStream && (
          <div className="bubble-row bubble-row-assistant">
            <div className="bubble bubble-assistant bubble-streaming">
              <div className="bubble-meta">
                <span>Hermes (응답 생성 중...)</span>
                <span className="pulse-dot" />
              </div>
              {liveStream.thinking && <div className="thinking-line">{liveStream.thinking}</div>}
              {liveStream.tools.length > 0 && (
                <div className="live-tools-strip">
                  {liveStream.tools.map((t, i) => (
                    <span key={i} className={`live-tool-chip ${t.status}`}>
                      {t.status === "running" ? "⏳" : "✓"} {t.name}
                    </span>
                  ))}
                </div>
              )}
              {liveStream.reasoning && (
                <details className="reasoning-details" open>
                  <summary>실시간 추론</summary>
                  <pre className="reasoning-pre">{liveStream.reasoning}</pre>
                </details>
              )}
              <div className="bubble-content">
                {liveStream.text ? renderFormattedText(liveStream.text) : "생각 중..."}
              </div>
            </div>
          </div>
        )}
      </div>

      <div className="composer-wrap">
        <div className="composer-box">
          <textarea
            className="composer-textarea"
            rows={2}
            placeholder={`${session.source} 세션(${session.effective_model})에 메시지 입력... (Enter 전송, Shift+Enter 줄바꿈)`}
            value={draft}
            onChange={(e) => setDraft(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === "Enter" && !e.shiftKey) {
                e.preventDefault();
                handleSubmit();
              }
            }}
          />

          <div className="composer-footer">
            <div className="composer-controls">
              <select
                className="compact-select"
                value={selectedProvider}
                onChange={(e) => onChangeProvider(e.target.value)}
                title="Provider"
              >
                {providers.map((p) => (
                  <option key={p.slug} value={p.slug}>
                    {p.slug}
                  </option>
                ))}
              </select>

              <select
                className="compact-select model-select"
                value={selectedModel}
                onChange={(e) => onChangeModel(e.target.value)}
                title={selectedModelDetail?.description || selectedModel}
              >
                {modelDetails.length > 0
                  ? modelDetails.map((m) => (
                      <option key={m.id} value={m.id}>
                        {m.display_name} ({m.id}
                        {m.context_window ? ` · ${formatCtx(m.context_window)}` : ""})
                      </option>
                    ))
                  : (currentProvider?.models ?? []).map((id) => (
                      <option key={id} value={id}>
                        {id}
                      </option>
                    ))}
              </select>

              <select
                className="compact-select"
                value={selectedEffort}
                onChange={(e) => onChangeEffort(e.target.value)}
                title="Reasoning Effort"
              >
                {availableEfforts.map((item) => (
                  <option key={item.effort} value={item.effort}>
                    effort: {item.effort}
                  </option>
                ))}
              </select>

              <button
                className="btn-secondary btn-xs"
                type="button"
                disabled={busy}
                onClick={onApplySessionModel}
                title="현재 열린 이 세션의 모델만 즉시 변경"
              >
                세션 적용
              </button>

              <button
                className="btn-secondary btn-xs"
                type="button"
                disabled={busy}
                onClick={onApplyGlobalModel}
                title="config.yaml 전역 기본값 + 텔레그램/활성 세션 모두 즉시 동기화"
              >
                전역+활성 동기화
              </button>
            </div>

            <div className="composer-send-group">
              {isStreaming ? (
                <button className="btn-danger btn-sm" type="button" onClick={onInterrupt}>
                  중단
                </button>
              ) : (
                <button
                  className="btn-primary btn-sm"
                  type="button"
                  disabled={!draft.trim() || busy}
                  onClick={handleSubmit}
                >
                  전송
                </button>
              )}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
