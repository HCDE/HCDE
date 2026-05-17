# HCDE Shadow Renderer Stage 7 - Shadowmap Light Priority

Stage 7 makes the Stage 4 shadowmap budget choose better lights. Before this
stage, the first active shadowmapped lights in the level's linked list received
shadowmap rows until the budget ran out. That made the budget predictable, but
not always useful in busy mods.

## User-facing controls

The new archived global CVAR is:

```text
gl_shadowmap_prioritize
```

Values:

| Value | Behavior |
| ---: | --- |
| `true` | Rank active shadowmapped lights before assigning rows. |
| `false` | Use the old level-list order. |

The Dynamic Lights menu now exposes `Prioritize shadowmapped lights` near the
shadowmap budget slider.

## Priority rule

HCDE ranks active shadowmapped lights with a simple local score:

```text
distance from viewer / light radius
```

Nearby lights win, and larger nearby lights can outrank small lights because
their shadows affect more visible space. Ties stay stable, so equal-priority
lights keep their previous order.

## Compatibility boundary

This stage only changes which lights receive the limited shadowmap rows. It does
not change the shadow shader, light definitions, actor lighting, or map geometry
queries.

Stage 8 adds `stat shadowmap` counters so this priority pass can be validated
from the console.
