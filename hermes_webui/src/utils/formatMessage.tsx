import { Fragment, useState, type ReactNode } from "react";

const IMAGE_EXT_RE = /\.(png|jpe?g|gif|webp|bmp|svg)$/i;
const IMAGE_ATTACHED_RE = /^\[Image attached at:\s*(.+?)\]\s*$/;
const MEDIA_PREFIX_RE = /^MEDIA:\s*(.+?)\s*$/;
const HEADING_RE = /^(#{1,3})\s+(.+)$/;

const INLINE_TOKEN_RE =
  /(`[^`\n]+`|!\[[^\]]*\]\([^)\n]+\)|\[[^\]]+\]\(https?:\/\/[^\s)\n]+\)|\*\*[^*\n]+?\*\*)/g;

function fileNameFromPath(rawPath: string): string {
  const normalized = rawPath.replace(/\\/g, "/");
  const idx = normalized.lastIndexOf("/");
  return idx >= 0 ? normalized.slice(idx + 1) : normalized;
}

function resolveMediaSrc(rawTarget: string): string | null {
  const trimmed = rawTarget.trim();
  if (!trimmed) return null;
  if (/^(https?:\/\/|data:image\/|\/api\/)/i.test(trimmed)) {
    return trimmed;
  }
  return `/api/media?path=${encodeURIComponent(trimmed)}`;
}

function AttachmentImage({ target, label }: { target: string; label?: string }) {
  const [failed, setFailed] = useState(false);
  const src = resolveMediaSrc(target);
  const name = label?.trim() || fileNameFromPath(target);

  if (!src || failed) {
    return (
      <div className="chat-attachment-chip" title={target}>
        <span>🖼️ 첨부 이미지: {name}</span>
        {failed && <span className="chat-attachment-muted">(캐시 파일 없음/만료)</span>}
      </div>
    );
  }

  return (
    <div className="chat-attachment">
      <img
        className="chat-attachment-img"
        src={src}
        alt={name}
        loading="lazy"
        onError={() => setFailed(true)}
      />
      <div className="chat-attachment-caption" title={target}>
        🖼️ {name}
      </div>
    </div>
  );
}

function renderInlineMarkdown(line: string): ReactNode[] {
  const nodes: ReactNode[] = [];
  let lastIndex = 0;
  INLINE_TOKEN_RE.lastIndex = 0;

  let match: RegExpExecArray | null;
  let keyIdx = 0;
  while ((match = INLINE_TOKEN_RE.exec(line)) !== null) {
    if (match.index > lastIndex) {
      nodes.push(line.slice(lastIndex, match.index));
    }
    const token = match[0];
    const key = `tok-${keyIdx++}`;

    if (token.startsWith("`") && token.endsWith("`")) {
      nodes.push(
        <code key={key} className="chat-inline-code">
          {token.slice(1, -1)}
        </code>
      );
    } else if (token.startsWith("![")) {
      const m = /^!\[([^\]]*)\]\(([^)\n]+)\)$/.exec(token);
      if (m) {
        nodes.push(<AttachmentImage key={key} target={m[2]} label={m[1]} />);
      } else {
        nodes.push(token);
      }
    } else if (token.startsWith("[")) {
      const m = /^\[([^\]]+)\]\((https?:\/\/[^\s)\n]+)\)$/.exec(token);
      if (m) {
        nodes.push(
          <a key={key} className="chat-link" href={m[2]} target="_blank" rel="noopener noreferrer">
            {m[1]}
          </a>
        );
      } else {
        nodes.push(token);
      }
    } else if (token.startsWith("**") && token.endsWith("**")) {
      nodes.push(<strong key={key}>{token.slice(2, -2)}</strong>);
    } else {
      nodes.push(token);
    }

    lastIndex = match.index + token.length;
  }

  if (lastIndex < line.length) {
    nodes.push(line.slice(lastIndex));
  }
  return nodes;
}

function renderTextBlock(text: string, blockKey: number): ReactNode {
  const lines = text.split("\n");
  return (
    <span key={blockKey} className="chat-plain-text">
      {lines.map((line, lineIdx) => {
        const attachMatch = IMAGE_ATTACHED_RE.exec(line) || MEDIA_PREFIX_RE.exec(line);
        if (attachMatch) {
          const target = attachMatch[1].trim();
          if (IMAGE_EXT_RE.test(target) || /^https?:\/\/|^data:image\//i.test(target)) {
            return <AttachmentImage key={lineIdx} target={target} />;
          }
          return (
            <div key={lineIdx} className="chat-attachment-chip" title={target}>
              📎 첨부 파일: {fileNameFromPath(target)}
            </div>
          );
        }

        const headingMatch = HEADING_RE.exec(line);
        if (headingMatch) {
          const level = headingMatch[1].length;
          return (
            <div key={lineIdx} className={`chat-heading chat-heading-${level}`}>
              {renderInlineMarkdown(headingMatch[2])}
            </div>
          );
        }

        const needsBreak = lineIdx < lines.length - 1;
        return (
          <Fragment key={lineIdx}>
            {renderInlineMarkdown(line)}
            {needsBreak ? "\n" : null}
          </Fragment>
        );
      })}
    </span>
  );
}

/** Render fenced code blocks, inline markdown formatting, links, and attached image previews. */
export function renderFormattedMessage(text: string): ReactNode {
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
    if (!part) return null;
    return renderTextBlock(part, idx);
  });
}

