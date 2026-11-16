export interface PlatformState {
  state?: string;
  error_message?: string | null;
}

export interface SessionItem {
  id: string;
  source: string;
  session_key: string;
  display_name: string | null;
  title: string | null;
  stored_model: string | null;
  effective_model: string;
  effective_provider: string;
  has_override: boolean;
  override_model: string | null;
  is_active: boolean;
  is_streaming?: boolean;
  last_activity_at: number | null;
  message_count: number;
  api_call_count: number;
  input_tokens: number;
  output_tokens: number;
  cache_read_tokens: number;
  reasoning_tokens: number;
  runtime_revision: number;
  runtime_generation: number | null;
  cwd?: string | null;
}

export interface OverviewResponse {
  hermes_home: string;
  gateway: {
    running: boolean;
    state: string;
    version: string;
    platforms: Record<string, PlatformState>;
  };
  global_model: {
    provider: string;
    model: string;
    reasoning_effort: string;
    effective_context_length: number;
  };
  telegram_session: SessionItem | null;
  sessions: SessionItem[];
}

export interface ReasoningLevelInfo {
  effort: string;
  description: string;
}

export interface ModelDetail {
  id: string;
  display_name: string;
  description: string;
  default_reasoning_level: string;
  supported_reasoning_levels: ReasoningLevelInfo[];
  visibility: string;
  context_window?: number | null;
  source: string;
}

export interface ProviderOption {
  slug: string;
  name: string;
  is_current?: boolean;
  models: string[];
  model_details?: ModelDetail[];
  authenticated?: boolean;
}

export interface ModelsResponse {
  providers: ProviderOption[];
  codex_catalog?: {
    source: string;
    fetched_at: string | null;
    client_version: string | null;
    cache_path: string;
    model_count: number;
  };
  reasoning_efforts: string[];
}

export interface ToolCallItem {
  id: string;
  name: string;
  arguments: string;
}

export interface MessageItem {
  id: number;
  role: string;
  content: string;
  tool_name: string | null;
  tool_calls: ToolCallItem[];
  tool_call_id: string | null;
  reasoning?: string;
  display_kind?: string | null;
  timestamp: number | null;
}

export interface LiveTurnStream {
  thinking: string;
  reasoning: string;
  text: string;
  tools: Array<{ name: string; status: "running" | "done"; detail?: string }>;
}

export interface McpServerItem {
  name: string;
  transport: string;
  command?: string | null;
  args?: string[];
  url?: string | null;
  enabled?: boolean;
}

export interface McpListResponse {
  servers: McpServerItem[];
  node_mcp_entry: string;
  node_mcp_built: boolean;
  node_mcp_name: string;
}

export interface McpToolProbeItem {
  name: string;
  description: string;
  schema_chars?: number;
}

export interface McpProbeResult {
  ok: boolean;
  error?: string;
  tools: McpToolProbeItem[];
}

export interface WorkspaceEntry {
  name: string;
  path: string;
  is_dir: boolean;
  size: number;
  mtime: number;
}
