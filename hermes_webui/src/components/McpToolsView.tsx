import { useState } from "react";
import type { McpListResponse, McpProbeResult } from "../types";

interface McpToolsViewProps {
  mcpData: McpListResponse | null;
  probeResult: McpProbeResult | null;
  toolOutput: unknown;
  busy: boolean;
  onRegisterNodeMcp: () => void;
  onProbeMcp: (name: string) => void;
  onCallTool: (toolName: string, args: Record<string, unknown>) => void;
}

export function McpToolsView({
  mcpData,
  probeResult,
  toolOutput,
  busy,
  onRegisterNodeMcp,
  onProbeMcp,
  onCallTool,
}: McpToolsViewProps) {
  const [randMin, setRandMin] = useState(1);
  const [randMax, setRandMax] = useState(100);
  const [randCount, setRandCount] = useState(3);
  const [customRoot, setCustomRoot] = useState("");

  return (
    <div className="page-scroll">
      <div className="page-header">
        <div>
          <h2>MCP 서버 관리 & TypeScript NodeMcpServer 테스트</h2>
          <p className="muted-text mono">
            {mcpData?.node_mcp_entry ?? "C:\\Users\\cbmplus\\source\\WinApp\\NodeMcpServer\\dist\\index.js"}
          </p>
        </div>
        <div className="row-gap">
          <button className="btn-primary btn-sm" onClick={onRegisterNodeMcp} disabled={busy} type="button">
            NodeMcpServer Hermes 등록/확인
          </button>
          <button
            className="btn-secondary btn-sm"
            onClick={() => onProbeMcp(mcpData?.node_mcp_name || "node_winapp_mcp")}
            disabled={busy}
            type="button"
          >
            Hermes Probe 연결 테스트
          </button>
        </div>
      </div>

      <div className="split-2">
        <div className="studio-card">
          <h3>등록된 Hermes MCP 서버</h3>
          {(mcpData?.servers ?? []).length === 0 ? (
            <p className="muted-text">등록된 MCP 서버가 없습니다.</p>
          ) : (
            <div className="mcp-server-list">
              {(mcpData?.servers ?? []).map((srv) => (
                <div key={srv.name} className="mcp-server-item">
                  <div>
                    <strong>{srv.name}</strong>{" "}
                    <span className="tag-chip">{srv.transport}</span>
                    <div className="mono muted-text" style={{ marginTop: 4 }}>
                      {srv.command} {(srv.args ?? []).join(" ")}
                    </div>
                  </div>
                  <button
                    className="btn-secondary btn-xs"
                    type="button"
                    disabled={busy}
                    onClick={() => onProbeMcp(srv.name)}
                  >
                    Probe
                  </button>
                </div>
              ))}
            </div>
          )}

          {probeResult && (
            <div style={{ marginTop: 14 }}>
              <h4>Hermes Probe 결과 ({probeResult.ok ? "성공" : "실패"})</h4>
              {probeResult.error && <div className="error-inline">{probeResult.error}</div>}
              <div className="mcp-tools-list">
                {probeResult.tools.map((t) => (
                  <div key={t.name} className="mcp-tool-doc">
                    <strong>{t.name}</strong>
                    <div className="muted-text">{t.description}</div>
                  </div>
                ))}
              </div>
            </div>
          )}
        </div>

        <div className="studio-card">
          <h3>NodeMcpServer 도구 직접 호출 테스트</h3>
          <div className="tool-runner-block">
            <div className="row-gap">
              <button
                className="btn-secondary btn-sm"
                type="button"
                disabled={busy}
                onClick={() => onCallTool("get_system_status", {})}
              >
                실행: get_system_status
              </button>
            </div>

            <div className="row-gap" style={{ marginTop: 10 }}>
              <input
                type="text"
                className="compact-input"
                placeholder="폴더 경로 (비우면 WinApp 루트)"
                value={customRoot}
                onChange={(e) => setCustomRoot(e.target.value)}
                style={{ flex: 1 }}
              />
              <button
                className="btn-secondary btn-sm"
                type="button"
                disabled={busy}
                onClick={() =>
                  onCallTool("list_winapp_projects", customRoot.trim() ? { root: customRoot.trim() } : {})
                }
              >
                실행: list_winapp_projects
              </button>
            </div>

            <div className="row-gap" style={{ marginTop: 10 }}>
              <input
                type="number"
                className="compact-input"
                value={randMin}
                onChange={(e) => setRandMin(Number(e.target.value))}
                style={{ width: 76 }}
              />
              <input
                type="number"
                className="compact-input"
                value={randMax}
                onChange={(e) => setRandMax(Number(e.target.value))}
                style={{ width: 76 }}
              />
              <input
                type="number"
                className="compact-input"
                value={randCount}
                onChange={(e) => setRandCount(Number(e.target.value))}
                style={{ width: 68 }}
              />
              <button
                className="btn-secondary btn-sm"
                type="button"
                disabled={busy}
                onClick={() =>
                  onCallTool("generate_random_number", {
                    min: randMin,
                    max: randMax,
                    count: randCount,
                  })
                }
              >
                실행: generate_random_number
              </button>
            </div>
          </div>

          <pre className="json-preview">
            {toolOutput !== null ? JSON.stringify(toolOutput, null, 2) : "도구를 실행하면 결과가 여기에 표시됩니다."}
          </pre>
        </div>
      </div>
    </div>
  );
}
