---
name: Ceres Edge
colors:
  surface: '#f9f9fd'
  surface-dim: '#d9dade'
  surface-bright: '#f9f9fd'
  surface-container-lowest: '#ffffff'
  surface-container-low: '#f3f3f7'
  surface-container: '#eeedf2'
  surface-container-high: '#e8e8ec'
  surface-container-highest: '#e2e2e6'
  on-surface: '#1a1c1f'
  on-surface-variant: '#3d4a3e'
  inverse-surface: '#2f3034'
  inverse-on-surface: '#f0f0f4'
  outline: '#6c7b6d'
  outline-variant: '#bbcbbb'
  surface-tint: '#006d37'
  primary: '#006d37'
  on-primary: '#ffffff'
  primary-container: '#2ecc71'
  on-primary-container: '#005027'
  inverse-primary: '#4ae183'
  secondary: '#006397'
  on-secondary: '#ffffff'
  secondary-container: '#5cb8fd'
  on-secondary-container: '#00476e'
  tertiary: '#735c00'
  on-tertiary: '#ffffff'
  tertiary-container: '#d7ae00'
  on-tertiary-container: '#544300'
  error: '#ba1a1a'
  on-error: '#ffffff'
  error-container: '#ffdad6'
  on-error-container: '#93000a'
  primary-fixed: '#6bfe9c'
  primary-fixed-dim: '#4ae183'
  on-primary-fixed: '#00210c'
  on-primary-fixed-variant: '#005228'
  secondary-fixed: '#cce5ff'
  secondary-fixed-dim: '#92ccff'
  on-secondary-fixed: '#001d31'
  on-secondary-fixed-variant: '#004b73'
  tertiary-fixed: '#ffe084'
  tertiary-fixed-dim: '#eec209'
  on-tertiary-fixed: '#231b00'
  on-tertiary-fixed-variant: '#574500'
  background: '#f9f9fd'
  on-background: '#1a1c1f'
  surface-variant: '#e2e2e6'
typography:
  display-lg:
    fontFamily: Hanken Grotesk
    fontSize: 48px
    fontWeight: '700'
    lineHeight: 56px
    letterSpacing: -0.02em
  headline-md:
    fontFamily: Hanken Grotesk
    fontSize: 24px
    fontWeight: '600'
    lineHeight: 32px
  headline-sm:
    fontFamily: Hanken Grotesk
    fontSize: 18px
    fontWeight: '600'
    lineHeight: 24px
  body-md:
    fontFamily: Hanken Grotesk
    fontSize: 16px
    fontWeight: '400'
    lineHeight: 24px
  data-mono:
    fontFamily: JetBrains Mono
    fontSize: 14px
    fontWeight: '500'
    lineHeight: 20px
    letterSpacing: 0.02em
  label-caps:
    fontFamily: JetBrains Mono
    fontSize: 11px
    fontWeight: '700'
    lineHeight: 16px
    letterSpacing: 0.08em
rounded:
  sm: 0.125rem
  DEFAULT: 0.25rem
  md: 0.375rem
  lg: 0.5rem
  xl: 0.75rem
  full: 9999px
spacing:
  unit: 4px
  gutter: 16px
  margin-mobile: 16px
  margin-desktop: 32px
  container-max: 1440px
---

## Brand & Style

This design system embodies the intersection of industrial hardware and high-precision software. The brand personality is authoritative, resilient, and highly technical, designed for operators who manage complex agricultural ecosystems through edge computing interfaces. 

The visual style is a blend of **Corporate Modern** and **Tactile Minimalism**. It leverages the "Jetson Nano" aesthetic—utilizing structured grids, subtle metallic gradients, and high-visibility status indicators. The UI should feel like a premium piece of hardware: dense with information but impeccably organized. The emotional response is one of total control and systemic reliability, ensuring that critical sensor data is never obscured by decorative elements.

## Colors

The palette is rooted in a clean, high-clarity light environment to maximize readability in bright industrial or outdoor settings.

- **Primary (Circuit Green):** Used exclusively for "Active," "Normal," and "Safe" states, as well as primary action buttons.
- **Secondary (Tech Blue):** Reserved for data visualization, interactive telemetry elements, and secondary navigation.
- **Tertiary (Alert Gold):** Used for warning states and non-critical maintenance notifications.
- **Neutral Surface:** A series of professional greys and off-whites provide the structural "chassis" of the interface, separating control panels from the background.

Status indicators should mimic physical LEDs, using the primary and secondary colors with a subtle 4px outer glow of the same hue to simulate light emission even in a light-mode environment.

## Typography

The typography system prioritizes legibility under various lighting conditions. 

- **Hanken Grotesk** serves as the primary typeface for its sharp, contemporary terminals and excellent vertical rhythm. It handles all structural headers and body copy.
- **JetBrains Mono** is utilized for all "active" data points, sensor readings, and timestamps. This monospaced font ensures that fluctuating numbers do not cause layout shifts and maintains a technical, industrial feel.

Mobile scales: For `display-lg`, reduce to `32px` on devices below 768px. All `data-mono` elements must maintain a minimum size of `12px` to ensure field readability.

## Layout & Spacing

The design system employs a **Fixed Grid** approach for the main dashboard to mimic a physical hardware console, transitioning to a fluid stack for mobile devices. 

- **Desktop:** A 12-column grid with 16px gutters. Modules are sized in "slots" (e.g., 3-column, 6-column, or 12-column widths).
- **Spacing Rhythm:** Based on a 4px base unit. Internal card padding is strictly 20px (5 units) to maintain a dense, professional data environment.
- **Alignment:** All elements must align to the top-left of their respective grid containers to reinforce the structured, systematic nature of edge computing.

## Elevation & Depth

This design system avoids traditional drop shadows in favor of **Tonal Layers** and **Inner Outlines**. 

- **Level 0 (Background):** The base light surface (#F9FAFB).
- **Level 1 (Cards/Panels):** Defined by a very subtle container fill and a 1px solid border to separate modules.
- **Level 2 (Dropdowns/Modals):** These use a 1px border of the Primary or Secondary color at 30% opacity to indicate "active" focus, with a subtle backdrop blur (8px) to separate the element from the data beneath.

Depth is communicated through contrast and containment rather than heavy shadow, ensuring the UI feels "bolted down" and industrial.

## Shapes

The shape language is "Soft-Industrial." Components use a precision 4px (`0.25rem`) corner radius. This provides a subtle nod to machined hardware components—not as sharp as raw code editors, but far more disciplined than consumer-grade social apps. 

- **Buttons:** 4px radius.
- **Input Fields:** 4px radius.
- **Status LEDs:** Circular (50% radius) to differentiate them from interactive UI elements.

## Components

- **Cards:** The primary container. Must include a 1px top border of the secondary color if the card contains interactive data visualizations.
- **Buttons:** 
    - *Primary:* Solid Circuit Green with dark/contrast text.
    - *Ghost:* 1px Tech Blue border with Tech Blue text.
- **Status Indicators (LEDs):** A 10px circle. For "Live" states, use a CSS pulse animation on a 2px outer glow.
- **Inputs:** Clean white/off-white backgrounds with a 1px border. On focus, the border transitions to Tech Blue with a subtle inner glow.
- **Data Tables:** High-density, no vertical borders. Use horizontal zebra-striping with a subtle neutral tint for row separation. 
- **Telemetry Chips:** Small, monospaced labels used for sensor tags (e.g., `TEMP_01`), utilizing a "tag" shape with a left-side color bar indicating sensor health.