/** The strip under the hidden title bar (traffic lights): drags the window. */
export function TitleBar() {
  return <div className="drag sticky top-0 z-10 h-9 shrink-0" data-tauri-drag-region />;
}
