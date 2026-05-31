# gr0m-integrity — the color coding, in detail

This is the long-form companion to the legend in the [README](../README.md). It
explains how to *read* the overlay and how to act on it. It does **not** explain how
the engine works internally — see [On the internals](#on-the-internals).

## The one-sentence version

> gr0m-integrity paints the model's answer so you can see, at a glance, which words
> earned your trust and which words didn't.

## Reading the colors

### Plain text — cleared
Un-highlighted text passed the integrity checks. It wasn't flagged as fabricated or
unsupported. This is the default, and on a good answer most of the response stays
plain. Plain doesn't mean "proven true forever" — it means "nothing tripped the
detector." Read it as you normally would.

### Pink / red — flagged
A highlighted span is one the overlay believes is **fabricated, unsupported, or not
backed by any trusted source.** This is the signal to stop and verify before you rely
on it, quote it, or forward it. Flagging is deliberately span-level: it points at the
specific clause, not the whole message, so you know *exactly* where the answer went
off the rails.

### `�` markers — unverifiable tokens
Inside a flagged span you'll sometimes see `�` characters. These mark the individual
tokens that failed verification outright — typically the load-bearing specifics: a
name, a number, a date, a causal claim. They are the words with no basis the overlay
could find. Use their density as a severity dial: a few `�` is a shaky detail; a
cluster like `����` means the core of that claim is invented.

## How to act on it

| You see… | Do… |
|---|---|
| All plain | Proceed; nothing flagged. |
| One flagged clause | Verify that clause before using it; the rest is likely fine. |
| Flagged span with sparse `�` | Check the specific highlighted detail. |
| Flagged span dense with `�` | Treat the whole claim as invented; don't propagate it. |

## Why it's drawn in place

The overlay renders the verdict on the exact words it applies to, rather than as a
separate score or report. That keeps the judgment glanceable, surgical (one bad
clause doesn't condemn a good answer), and impossible to ignore while you read.

## On the internals

How spans are scored, how `�` tokens are isolated, and what the engine verifies
against are **intentionally undocumented.** This file is about how to *use* the
overlay. For commercial use, integration, or a deeper technical conversation under
NDA, reach out via **[groberman.tech](https://groberman.tech)**.
