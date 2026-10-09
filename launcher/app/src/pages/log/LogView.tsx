import { useEffect, useRef } from "react";

type Props = { lines: string[]; follow: boolean };

/** The game's output, monospaced; keeps the newest line in view while following. */
export function LogView({ lines, follow }: Props) {
  const end = useRef<HTMLDivElement>(null);
  useEffect(() => {
    if (follow) end.current?.scrollIntoView({ block: "end" });
  }, [lines, follow]);
  return (
    <div className="h-[calc(100vh-15rem)] min-h-64 overflow-auto rounded-2xl border border-border bg-night-950/80 p-4 font-mono text-[11px] leading-relaxed text-parchment/85 select-text">
      {lines.map((line, i) => (
        <div key={i} className={/error|assert|stop:/i.test(line) ? "whitespace-pre-wrap text-blood-400" : "whitespace-pre-wrap"}>
          {line}
        </div>
      ))}
      <div ref={end} />
    </div>
  );
}
