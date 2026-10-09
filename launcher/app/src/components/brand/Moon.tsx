/** The crescent of the app icon (launcher/macos/make_icon.swift). */
export function Moon({ className }: { className?: string }) {
  return (
    <svg viewBox="0 0 64 64" className={className} aria-hidden="true">
      <defs>
        <radialGradient id="moon-glow" cx="50%" cy="50%" r="50%">
          <stop offset="55%" stopColor="#f3e2c4" stopOpacity="0.25" />
          <stop offset="100%" stopColor="#f3e2c4" stopOpacity="0" />
        </radialGradient>
        <mask id="moon-cut">
          <rect width="64" height="64" fill="white" />
          <circle cx="39" cy="26" r="17" fill="black" />
        </mask>
      </defs>
      <circle cx="32" cy="32" r="30" fill="url(#moon-glow)" />
      <circle cx="32" cy="32" r="20" fill="#f5e8cf" mask="url(#moon-cut)" />
    </svg>
  );
}
