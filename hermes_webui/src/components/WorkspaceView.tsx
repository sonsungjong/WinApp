import { useCallback, useEffect, useState } from "react";
import type { WorkspaceEntry } from "../types";

interface WorkspaceViewProps {
  apiFetch: <T>(url: string, init?: RequestInit) => Promise<T>;
}

export function WorkspaceView({ apiFetch }: WorkspaceViewProps) {
  const [root, setRoot] = useState<"winapp" | "hermes">("winapp");
  const [subpath, setSubpath] = useState("");
  const [entries, setEntries] = useState<WorkspaceEntry[]>([]);
  const [selectedFile, setSelectedFile] = useState<{
    path: string;
    content: string;
    size: number;
    truncated: boolean;
  } | null>(null);
  const [error, setError] = useState<string | null>(null);

  const loadDir = useCallback(
    async (targetRoot: "winapp" | "hermes", targetSubpath: string) => {
      try {
        setError(null);
        const data = await apiFetch<{ entries: WorkspaceEntry[]; subpath: string }>(
          `/api/workspace/files?root=${targetRoot}&subpath=${encodeURIComponent(targetSubpath)}`
        );
        setEntries(data.entries);
        setSubpath(data.subpath);
      } catch (err) {
        setError((err as Error).message);
      }
    },
    [apiFetch]
  );

  useEffect(() => {
    loadDir(root, "");
  }, [root, loadDir]);

  const openEntry = async (entry: WorkspaceEntry) => {
    if (entry.is_dir) {
      loadDir(root, entry.path);
      return;
    }
    try {
      setError(null);
      const file = await apiFetch<{
        path: string;
        content: string;
        size: number;
        truncated: boolean;
      }>(`/api/workspace/file?root=${root}&path=${encodeURIComponent(entry.path)}`);
      setSelectedFile(file);
    } catch (err) {
      setError((err as Error).message);
    }
  };

  const goUp = () => {
    if (!subpath) return;
    const parts = subpath.split("/").filter(Boolean);
    parts.pop();
    loadDir(root, parts.join("/"));
  };

  return (
    <div className="page-scroll">
      <div className="page-header">
        <div>
          <h2>워크스페이스 파일 탐색기</h2>
          <p className="muted-text mono">
            /{root}
            {subpath ? `/${subpath}` : ""}
          </p>
        </div>
        <div className="row-gap">
          <button
            className={`btn-secondary btn-sm ${root === "winapp" ? "active-tab-btn" : ""}`}
            type="button"
            onClick={() => {
              setSelectedFile(null);
              setRoot("winapp");
            }}
          >
            WinApp 소스
          </button>
          <button
            className={`btn-secondary btn-sm ${root === "hermes" ? "active-tab-btn" : ""}`}
            type="button"
            onClick={() => {
              setSelectedFile(null);
              setRoot("hermes");
            }}
          >
            Hermes Workspace
          </button>
          <button className="btn-secondary btn-sm" type="button" onClick={goUp} disabled={!subpath}>
            상위 폴더
          </button>
        </div>
      </div>

      {error && <div className="error-inline">{error}</div>}

      <div className="split-files">
        <div className="studio-card file-list-card">
          {entries.map((entry) => (
            <button
              key={entry.path}
              type="button"
              className={`file-row ${selectedFile?.path === entry.path ? "file-row-active" : ""}`}
              onClick={() => openEntry(entry)}
            >
              <span>
                {entry.is_dir ? "📁" : "📄"} {entry.name}
              </span>
              <span className="muted-text mono">{entry.is_dir ? "DIR" : `${entry.size} B`}</span>
            </button>
          ))}
        </div>

        <div className="studio-card file-preview-card">
          {selectedFile ? (
            <>
              <div className="file-preview-head">
                <strong className="mono">{selectedFile.path}</strong>
                <span className="muted-text">
                  {selectedFile.size} bytes {selectedFile.truncated ? "(64KB 미리보기)" : ""}
                </span>
              </div>
              <pre className="json-preview file-code-preview">{selectedFile.content}</pre>
            </>
          ) : (
            <div className="muted-text">왼쪽에서 파일을 클릭하면 코드 내용을 미리볼 수 있습니다.</div>
          )}
        </div>
      </div>
    </div>
  );
}
