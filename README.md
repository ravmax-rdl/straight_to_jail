```
 ________  _________  ________  ________  ___  ________  ___  ___  _________       
|\   ____\|\___   ___\\   __  \|\   __  \|\  \|\   ____\|\  \|\  \|\___   ___\     
\ \  \___|\|___ \  \_\ \  \|\  \ \  \|\  \ \  \ \  \___|\ \  \\\  \|___ \  \_|     
 \ \_____  \   \ \  \ \ \   _  _\ \   __  \ \  \ \  \  __\ \   __  \   \ \  \      
  \|____|\  \   \ \  \ \ \  \\  \\ \  \ \  \ \  \ \  \|\  \ \  \ \  \   \ \  \     
    ____\_\  \   \ \__\ \ \__\\ _\\ \__\ \__\ \__\ \_______\ \__\ \__\   \ \__\    
   |\_________\   \|__|  \|__|\|__|\|__|\|__|\|__|\|_______|\|__|\|__|    \|__|    
   \|_________|                                                                    
                                                                                   
                                                                                   
 _________  ________             ___  ________  ___  ___                           
|\___   ___\\   __  \           |\  \|\   __  \|\  \|\  \                          
\|___ \  \_\ \  \|\  \          \ \  \ \  \|\  \ \  \ \  \                         
     \ \  \ \ \  \\\  \       __ \ \  \ \   __  \ \  \ \  \                        
      \ \  \ \ \  \\\  \     |\  \\_\  \ \  \ \  \ \  \ \  \____                   
       \ \__\ \ \_______\    \ \________\ \__\ \__\ \__\ \_______\                 
        \|__|  \|_______|     \|________|\|__|\|__|\|__|\|_______|                 
                                                                                   
                                                                                   
                                                                                             
```

</br>

<p align="center" ><img height="20px" src="https://ziadoua.github.io/m3-Markdown-Badges/badges/C/c2.svg"> <img height="20px" src="https://ziadoua.github.io/m3-Markdown-Badges/badges/Linux/linux3.svg"> <img height="20px" src="https://ziadoua.github.io/m3-Markdown-Badges/badges/Windows/windows3.svg">
</p>

**MONOPOLY-LK** — a fully autonomous, Sri-Lanka-themed Monopoly economic simulation in pure C. Four AI players with distinct financial personalities battle across a 40-square Colombo-to-Jaffna board through loans, insurance, inflation, disasters, and market swings. Zero user input: launch it and watch an economy play itself out over up to 500 rounds.

Take-home assignment for *SCS 1301 – Data Structures and Program Design using C* (University of Colombo School of Computing). The full ruleset lives in [`assets/Assignment_1_unlocked.pdf`](assets/Assignment_1_unlocked.pdf).

## The game

Classic Monopoly bones — 22 properties in 8 colour groups, 4 railway stations, 2 utilities, auctions, monopolies, houses and hotels, jail — grafted onto a working model of the Sri Lankan economy:

- **Banking** — secured loans from Bank of Ceylon at 75% loan-to-value, interest compounding every round, refinancing, and foreclosure when it all goes wrong.
- **Insurance** — three policy tiers (Basic / Comprehensive / Business Interruption) from two insurers, protecting against the fires, floods, and riots that randomly strike developed properties.
- **A living economy** — periodic inflation draws reprice everything; property groups boom and crash on a rolling market cycle; buildings age, decay, and demand maintenance or suffer structural damage.
- **Events on independent timers** — national economic events every 15 rounds, regional development cards, government regulations every 20 rounds, and a 20-card event deck drawn on Event squares — all stacking cumulatively on the same shared prices.

Last solvent player wins, or highest net worth after 500 rounds.

## The players

| Player | Personality |
|--------|-------------|
| **Aggressive Investor** | Expand fast, build max houses then hotels, leverage debt for rental income |
| **Conservative Banker** | Preserve capital, over-insure, hoard cash, distrust loans |
| **Risk Taker** | Buy everything, borrow the maximum, bid until the wallet is empty |
| **Opportunistic Trader** | Read the market — buy into booms, build under subsidies, sell before declines |

Every decision — purchases, bids, loans, insurance, construction, renovation — is made programmatically by these four engines.

## Architecture

| File | Responsibility |
|------|----------------|
| `types.h` | Shared structs, enums, constants, prototypes |
| `board.c` | The 40 squares, movement, rent/value calculation |
| `players.c` | The four strategy decision engines |
| `finance.c` | Loans, insurance, auctions, taxes, depreciation, bankruptcy |
| `events.c` | Economic events, event cards, regulations, disasters, market cycles |
| `game.c` | Round/turn controller and the economic-cadence scheduler |
| `main.c` | Entry point |

One `GameState` struct threads through everything — no globals, no dynamic allocation, integer-only money. The heart of the simulation is a scheduler that fires interacting economic systems on staggered cycles (every 1 / 5 / 10 / 15 / 20 rounds) against shared, cumulatively-modified prices.

## Build

```bash
make            # or the canonical grading build:
gcc *.c -o monopoly
```

## Run

```bash
./straight_to_jail        # make target
./monopoly [seed]         # canonical binary; optional PRNG seed for reproducible runs
```

## Status

🚧 **Planning phase** — no C source yet. Prep documents:

| Document | What it is |
|----------|------------|
| [`docs/REQUIREMENTS.md`](docs/REQUIREMENTS.md) | Requirements checklist traced to spec rule numbers, plus the 15 spec-gap decisions (D1–D15) resolved before building |
| [`docs/superpowers/specs/…-architecture-design.md`](docs/superpowers/specs/2026-07-28-straight-to-jail-architecture-design.md) | Architecture rationale — the effect registry, the three choke points, the round scheduler |
| [`docs/superpowers/plans/…-staged.md`](docs/superpowers/plans/2026-07-28-straight-to-jail-staged.md) | **35-stage implementation plan** in 9 phases; every stage compiles clean and runs |
| [`docs/learning/`](docs/learning/) | Three reference documents — the C, the data structures, the economic mathematics — built to PDF with pandoc + LaTeX |

### Reference material

The learning documents explain the concepts each stage assumes, using this project's own code
and numbers:

- [`01-c-language.md`](docs/learning/01-c-language.md) — multi-file compilation, enums, structs,
  pointers, `const`-correctness, seeded randomness and modulo bias, and the five bugs this
  project invites
- [`02-program-design.md`](docs/learning/02-program-design.md) — modelling the board, the effect
  registry, choke points, the round scheduler, state machines, circular queues, and verifying
  without a test framework
- [`03-economic-math.md`](docs/learning/03-economic-math.md) — integer money, truncation,
  composing percentages, overflow headroom, compound interest, decay models, and expected value

Build the PDFs with `docs/learning/build.ps1` (Windows) or `docs/learning/build.sh` (POSIX).
Requires pandoc and a LaTeX engine — see [`docs/learning/README.md`](docs/learning/README.md).

<a href="/LICENSE"><img height="24px" src="https://ziadoua.github.io/m3-Markdown-Badges/badges/LicenceMIT/licencemit1.svg"></a>
