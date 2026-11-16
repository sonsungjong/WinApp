import type { ModelDetail, ModelsResponse, OverviewResponse } from "../types";

interface ModelsCatalogViewProps {
  overview: OverviewResponse | null;
  modelsData: ModelsResponse | null;
  selectedEffort: string;
  busy: boolean;
  onChangeEffort: (effort: string) => void;
  onRefreshCatalog: () => void;
  onApplyModel: (provider: string, model: string) => void;
}

export function ModelsCatalogView({
  overview,
  modelsData,
  selectedEffort,
  busy,
  onChangeEffort,
  onRefreshCatalog,
  onApplyModel,
}: ModelsCatalogViewProps) {
  const catalog = modelsData?.codex_catalog;
  const providers = modelsData?.providers ?? [];

  return (
    <div className="page-scroll">
      <div className="page-header">
        <div>
          <h2>실시간 모델 카탈로그 (Codex OAuth & Hermes Providers)</h2>
          <p className="muted-text">
            하드코딩 없이{" "}
            <span className="mono">{catalog?.cache_path || "~/.codex/models_cache.json"}</span> 및
            Hermes <span className="mono">/api/model/options</span>에서 실시간으로 가져온 모델 목록입니다.
          </p>
        </div>
        <div className="row-gap">
          <select
            className="compact-select"
            value={selectedEffort}
            onChange={(e) => onChangeEffort(e.target.value)}
          >
            {(modelsData?.reasoning_efforts ?? ["low", "medium", "high", "xhigh", "max", "ultra"]).map(
              (eff) => (
                <option key={eff} value={eff}>
                  적용 추론강도: {eff}
                </option>
              )
            )}
          </select>
          <button className="btn-primary btn-sm" onClick={onRefreshCatalog} disabled={busy} type="button">
            {busy ? "새로고침 중..." : "Codex 서버 카탈로그 새로고침"}
          </button>
        </div>
      </div>

      <div className="meta-strip">
        <span>소스: <strong>{catalog?.source ?? "-"}</strong></span>
        <span>Codex Client: <strong>{catalog?.client_version ?? "-"}</strong></span>
        <span>마지막 Fetch: <strong>{catalog?.fetched_at ?? "-"}</strong></span>
        <span>전역 기본: <strong>{overview?.global_model.model ?? "-"}</strong></span>
        <span>텔레그램 활성: <strong>{overview?.telegram_session?.effective_model ?? "-"}</strong></span>
      </div>

      {providers.map((prov) => (
        <div key={prov.slug} className="provider-section">
          <div className="provider-heading">
            <h3>
              {prov.name} <span className="mono muted-text">({prov.slug})</span>
            </h3>
            <span className="tag-chip">{prov.models.length} models</span>
          </div>

          <div className="catalog-grid">
            {(prov.model_details ?? []).map((item: ModelDetail) => {
              const isGlobal = overview?.global_model.model === item.id;
              const isTg = overview?.telegram_session?.effective_model === item.id;
              return (
                <div key={item.id} className={`catalog-card ${isGlobal || isTg ? "catalog-card-active" : ""}`}>
                  <div className="catalog-card-top">
                    <div>
                      <div className="catalog-title">{item.display_name}</div>
                      <div className="mono muted-text">{item.id}</div>
                    </div>
                    <div className="row-gap">
                      {item.context_window && (
                        <span className="tag-chip">{Math.round(item.context_window / 1000)}K</span>
                      )}
                      {isGlobal && <span className="tag-chip tag-blue">전역 기본</span>}
                      {isTg && <span className="tag-chip tag-green">TG 활성</span>}
                    </div>
                  </div>
                  <p className="catalog-desc">{item.description}</p>
                  <div className="effort-chips">
                    {item.supported_reasoning_levels.map((lvl) => (
                      <span key={lvl.effort} className="effort-chip" title={lvl.description}>
                        {lvl.effort}
                      </span>
                    ))}
                  </div>
                  <div className="catalog-card-actions">
                    <button
                      className="btn-secondary btn-xs"
                      type="button"
                      disabled={busy}
                      onClick={() => onApplyModel(prov.slug, item.id)}
                    >
                      전역 + 활성 세션에 즉시 적용
                    </button>
                  </div>
                </div>
              );
            })}
          </div>
        </div>
      ))}
    </div>
  );
}
