// The PS4 pad's inputs (src/runtime_pad.c): bbport.ini "key.<id>=" and "pad.<id>=" replace a
// default binding; without a line the default applies. pad: null = keyboard only. Labels:
// i18n controls.inputs.<id>, groups controls.groups.<group>.
export type ControlGroup = "Face buttons" | "Shoulders and sticks" | "System" | "D-pad" | "Movement and camera";

export type Control = {
  id: string;
  label: string;
  glyph?: string;
  key: string;
  pad: string | null;
  group: ControlGroup;
};

export const CONTROL_GROUPS: ControlGroup[] = ["Face buttons", "Shoulders and sticks", "System", "D-pad", "Movement and camera"];

export const CONTROLS: Control[] = [
  { id: "cross", label: "Cross", glyph: "✕", key: "Space", pad: "a", group: "Face buttons" },
  { id: "circle", label: "Circle", glyph: "○", key: "Left Shift", pad: "b", group: "Face buttons" },
  { id: "square", label: "Square", glyph: "□", key: "E", pad: "x", group: "Face buttons" },
  { id: "triangle", label: "Triangle", glyph: "△", key: "Q", pad: "y", group: "Face buttons" },
  { id: "l1", label: "L1", key: "1", pad: "leftshoulder", group: "Shoulders and sticks" },
  { id: "r1", label: "R1", key: "3", pad: "rightshoulder", group: "Shoulders and sticks" },
  { id: "l2", label: "L2", key: "R", pad: "lefttrigger", group: "Shoulders and sticks" },
  { id: "r2", label: "R2", key: "F", pad: "righttrigger", group: "Shoulders and sticks" },
  { id: "l3", label: "L3", key: "Z", pad: "leftstick", group: "Shoulders and sticks" },
  { id: "r3", label: "R3", key: "C", pad: "rightstick", group: "Shoulders and sticks" },
  { id: "options", label: "Options", key: "Return", pad: "start", group: "System" },
  { id: "touchpad", label: "Touchpad (left half)", key: "Tab", pad: "back, touchpad", group: "System" },
  { id: "touchpad_right", label: "Touchpad (right half)", key: "Backspace", pad: "", group: "System" },
  { id: "up", label: "D-pad up", key: "I", pad: "dpup", group: "D-pad" },
  { id: "down", label: "D-pad down", key: "K", pad: "dpdown", group: "D-pad" },
  { id: "left", label: "D-pad left", key: "J", pad: "dpleft", group: "D-pad" },
  { id: "right", label: "D-pad right", key: "L", pad: "dpright", group: "D-pad" },
  { id: "move_up", label: "Move forward", key: "W", pad: null, group: "Movement and camera" },
  { id: "move_down", label: "Move back", key: "S", pad: null, group: "Movement and camera" },
  { id: "move_left", label: "Move left", key: "A", pad: null, group: "Movement and camera" },
  { id: "move_right", label: "Move right", key: "D", pad: null, group: "Movement and camera" },
  { id: "look_up", label: "Camera up", key: "Up", pad: null, group: "Movement and camera" },
  { id: "look_down", label: "Camera down", key: "Down", pad: null, group: "Movement and camera" },
  { id: "look_left", label: "Camera left", key: "Left", pad: null, group: "Movement and camera" },
  { id: "look_right", label: "Camera right", key: "Right", pad: null, group: "Movement and camera" },
];

/** SDL gamepad button names, as PlayStation / Xbox labels. */
const PAD_NAMES: Record<string, string> = {
  a: "✕ / A",
  b: "○ / B",
  x: "□ / X",
  y: "△ / Y",
  leftshoulder: "L1 / LB",
  rightshoulder: "R1 / RB",
  lefttrigger: "L2 / LT",
  righttrigger: "R2 / RT",
  leftstick: "L3",
  rightstick: "R3",
  start: "Options / Menu",
  back: "Share / View",
  touchpad: "Touchpad",
  guide: "PS / Home",
  dpup: "D-pad ↑",
  dpdown: "D-pad ↓",
  dpleft: "D-pad ←",
  dpright: "D-pad →",
  misc1: "Mute / Share",
};

/** A gamepad binding ("back, touchpad") as button labels; `or` joins alternatives. */
export function padLabel(binding: string, or: string): string {
  return binding
    .split(",")
    .map((b) => b.trim())
    .filter(Boolean)
    .map((b) => PAD_NAMES[b] ?? b)
    .join(` ${or} `);
}
