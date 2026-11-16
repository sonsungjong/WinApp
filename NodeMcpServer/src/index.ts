import os from "node:os";
import fs from "node:fs/promises";
import path from "node:path";
import { Server } from "@modelcontextprotocol/sdk/server/index.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import {
  CallToolRequestSchema,
  ListToolsRequestSchema,
} from "@modelcontextprotocol/sdk/types.js";
import { z } from "zod";

const WINAPP_ROOT = "C:\\Users\\cbmplus\\source\\WinApp";

const RandomNumberArgsSchema = z.object({
  min: z.number().int().default(1),
  max: z.number().int().default(100),
  count: z.number().int().min(1).max(50).default(1),
});

const ListProjectsArgsSchema = z.object({
  root: z.string().optional(),
});

function formatGb(bytes: number): number {
  return Math.round((bytes / (1024 ** 3)) * 100) / 100;
}

async function detectProjectType(projectDir: string): Promise<string> {
  try {
    const entries = await fs.readdir(projectDir);
    if (entries.some((f) => f.endsWith(".csproj"))) return "C# (.NET)";
    if (entries.some((f) => f.endsWith(".vcxproj") || f === "CMakeLists.txt")) return "C++";
    if (entries.includes("vite.config.ts") || entries.includes("vite.config.js")) return "React/Vite";
    if (entries.includes("tsconfig.json")) return "TypeScript/Node";
    if (entries.includes("package.json")) return "Node.js";
    if (entries.includes("main.py") || entries.includes("requirements.txt")) return "Python/FastAPI";
    if (entries.length === 0) return "Empty";
    return "Other";
  } catch {
    return "Unknown";
  }
}

const server = new Server(
  {
    name: "node-winapp-mcp",
    version: "1.0.0",
  },
  {
    capabilities: {
      tools: {},
    },
  }
);

server.setRequestHandler(ListToolsRequestSchema, async () => ({
  tools: [
    {
      name: "get_system_status",
      description: "Returns Windows OS, hostname, CPU, memory (GB), and uptime details.",
      inputSchema: {
        type: "object",
        properties: {},
      },
    },
    {
      name: "generate_random_number",
      description: "Generates random integers between min and max (inclusive).",
      inputSchema: {
        type: "object",
        properties: {
          min: {
            type: "integer",
            description: "Minimum integer (inclusive, default 1)",
            default: 1,
          },
          max: {
            type: "integer",
            description: "Maximum integer (inclusive, default 100)",
            default: 100,
          },
          count: {
            type: "integer",
            description: "How many random integers to generate (1..50, default 1)",
            default: 1,
          },
        },
      },
    },
    {
      name: "list_winapp_projects",
      description: "Lists directories under C:\\Users\\cbmplus\\source\\WinApp and detects each project type.",
      inputSchema: {
        type: "object",
        properties: {
          root: {
            type: "string",
            description: "Optional custom directory path to inspect (defaults to WinApp root).",
          },
        },
      },
    },
  ],
}));

server.setRequestHandler(CallToolRequestSchema, async (request) => {
  const { name, arguments: rawArgs } = request.params;

  if (name === "get_system_status") {
    const cpus = os.cpus();
    const payload = {
      platform: os.platform(),
      release: os.release(),
      arch: os.arch(),
      hostname: os.hostname(),
      cpu_model: cpus[0]?.model ?? "Unknown",
      cpu_cores: cpus.length,
      total_ram_gb: formatGb(os.totalmem()),
      free_ram_gb: formatGb(os.freemem()),
      uptime_seconds: Math.round(os.uptime()),
      timestamp: new Date().toISOString(),
    };
    return {
      content: [{ type: "text", text: JSON.stringify(payload, null, 2) }],
    };
  }

  if (name === "generate_random_number") {
    const parsed = RandomNumberArgsSchema.safeParse(rawArgs ?? {});
    if (!parsed.success) {
      return {
        isError: true,
        content: [{ type: "text", text: `Invalid arguments: ${parsed.error.message}` }],
      };
    }
    const { min, max, count } = parsed.data;
    if (min > max) {
      return {
        isError: true,
        content: [{ type: "text", text: `min (${min}) cannot be greater than max (${max})` }],
      };
    }
    const numbers: number[] = [];
    for (let i = 0; i < count; i++) {
      numbers.push(Math.floor(Math.random() * (max - min + 1)) + min);
    }
    const payload = { min, max, count, numbers };
    return {
      content: [{ type: "text", text: JSON.stringify(payload, null, 2) }],
    };
  }

  if (name === "list_winapp_projects") {
    const parsed = ListProjectsArgsSchema.safeParse(rawArgs ?? {});
    if (!parsed.success) {
      return {
        isError: true,
        content: [{ type: "text", text: `Invalid arguments: ${parsed.error.message}` }],
      };
    }
    const targetRoot = parsed.data.root?.trim() || WINAPP_ROOT;
    const entries = await fs.readdir(targetRoot, { withFileTypes: true });
    const projects: Array<{ name: string; type: string; path: string }> = [];
    for (const entry of entries) {
      if (!entry.isDirectory() || entry.name.startsWith(".")) continue;
      const fullPath = path.join(targetRoot, entry.name);
      const projectType = await detectProjectType(fullPath);
      projects.push({ name: entry.name, type: projectType, path: fullPath });
    }
    projects.sort((a, b) => a.name.localeCompare(b.name));
    return {
      content: [{ type: "text", text: JSON.stringify({ root: targetRoot, count: projects.length, projects }, null, 2) }],
    };
  }

  return {
    isError: true,
    content: [{ type: "text", text: `Unknown tool: ${name}` }],
  };
});

async function main(): Promise<void> {
  const transport = new StdioServerTransport();
  await server.connect(transport);
}

main().catch((err) => {
  console.error("NodeMcpServer fatal error:", err);
  process.exit(1);
});
