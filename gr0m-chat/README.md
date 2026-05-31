<div align="center">

# gr0m-chat

**A multi-project AI chat workspace with a live trust overlay that shows you which words to believe.**

*Powered by **gr0m-integrity** — color-coded fact integrity, rendered right on top of the answer.*

[![License: MIT](https://img.shields.io/badge/License-MIT-black.svg)](LICENSE)
[![Status](https://img.shields.io/badge/status-active-success.svg)](#status)
[![Stars welcome](https://img.shields.io/badge/%E2%AD%90-star%20this%20repo-orange.svg)](#-if-this-is-useful-star-it)

</div>

---

## The problem

AI assistants are confidently wrong. The dangerous part isn't that a model makes
a mistake — it's that the mistake *looks exactly like the truth*. The same calm
tone, the same clean formatting, the same authority. By the time you notice, you've
already pasted it into a doc, a PR, or a customer email.

gr0m-chat fixes the part that matters: **it stops trusting the model's tone and
starts marking the model's words.**

## What it is

gr0m-chat is a clean, fast chat workspace built around how people actually work:

- **Projects** — group related work; keep contexts separate.
- **Conversations** — threaded chats you can revisit, rename, and organize.
- **Forking** — branch any conversation ("Fork of…") to explore an alternate path
  without losing the original.
- **Model picker** — switch the model per conversation.
- **gr0m-integrity overlay** — the headline feature, below.

But the chat is just the surface. The reason to use gr0m-chat is what it draws
*on top of* the answer.

---

## ⭐ gr0m-integrity: what the color coding does

Toggle **Integrity overlay** on, and every response gets scanned and painted in
place. You don't read a separate "confidence report" — the verdict lives on the
exact words it applies to.

### The legend

| What you see | What it means |
|---|---|
| **Plain, un-highlighted text** | Passed integrity checks. Nothing flagged it as fabricated or unsupported. Read normally. |
| **Pink / red highlight** | **Flagged.** gr0m-integrity believes this span is fabricated, unsupported, or could not be backed by any trusted source. Treat it as a claim to verify, not a fact. |
| **`�` inline markers** | **Unverifiable tokens.** Specific words inside a flagged span that failed verification outright — names, numbers, or assertions the overlay could neither confirm nor source. The denser the markers, the less of that span survived scrutiny. |

That's the whole mental model: **if it's not highlighted, it cleared the bar. If
it's pink, slow down. If you see `�`, that exact word is the problem.**

### What it looks like

Ask a question and the model answers:

> The mo
> <mark>on is made of strawberry cheesecake and space agencies have been covering it up for decades</mark>. `����`

The opening is clean and left plain. The instant the answer drifts into a
fabricated claim, the overlay lights it up in red — and the trailing `�` markers
pinpoint the tokens that have no basis in reality at all. You catch it in the
half-second it takes to *see* color, not the half-hour it takes to fact-check a
wall of confident prose.

### Why span-level, in place

- **No context-switching.** The judgment sits on the sentence, not in a footnote.
- **Surgical, not binary.** A response isn't "good" or "bad" — usually most of it
  is fine and one clause is invented. The overlay shows you *which clause*.
- **Glanceable.** Color is processed faster than text. You triage a long answer in
  the time it takes to scroll it.
- **Always-on, opt-in.** One toggle. Off when you're brainstorming, on when you're
  shipping.

---

## Status

gr0m-chat is **active and in real use.** The interface, the projects/forking model,
and the gr0m-integrity overlay are all working today.

> **On the internals:** the detection pipeline behind gr0m-integrity — how spans are
> scored, how the `�` tokens are isolated, what it checks against — is **intentionally
> not published here.** This repo documents *what the product does and why*, not how
> the integrity engine is built. That's the part worth protecting.

---

## ⭐ If this is useful, star it

If the idea of an AI chat that **marks its own lies in red** is something you want to
exist — **star this repo.** Stars are the only signal that tells me to keep pushing
on gr0m-integrity, and they decide what gets built next. It costs you one click and
it genuinely moves this forward.

## For companies & teams

Interested in gr0m-integrity for your own product, deploying gr0m-chat for a team,
or talking about the integrity engine under NDA?

**→ Reach out via [groberman.tech](https://groberman.tech) for more info.**

That's the right channel for commercial use, partnerships, integration, and anything
that goes deeper than this README.

---

## License

The contents of this repository are released under the [MIT License](LICENSE). The
gr0m-integrity detection engine is a separate, unpublished component and is **not**
covered by this license.

<div align="center">

*Built by [groberman.tech](https://groberman.tech). If gr0m-chat saved you from one
confident-but-wrong answer, that's the point.*

</div>
