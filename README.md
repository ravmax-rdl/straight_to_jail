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

- **Banking** — secured loans from Bank of Ceylon at 75% loan-to-value, interest compounding every round, mid-term top-ups, and foreclosure when it all goes wrong.
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
| `finance.c` | Loans, insurance, auctions, taxes, debt recovery, bankruptcy |
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
./monopoly                       # random seed
./monopoly 42                    # fixed seed — reproducible, byte for byte
./monopoly 42 path/to/Rent.csv   # explicit data file
```

Per-property purchase prices and base rents are **read from
[`assets/Rent.csv`](assets/Rent.csv) at runtime**, not compiled in — edit a price there and the
next run uses it, no rebuild needed.

The file is looked for at `assets/Rent.csv`, then `Rent.csv`, then `../assets/Rent.csv`, so the
program works from either the repository root or the source directory. Give a path as the second
argument to override the search. If the file cannot be found or is malformed, the program prints
a diagnostic to `stderr` and exits 1 without writing anything to `stdout`.

## Verifying a build

There is no test binary — the mandated `gcc *.c -o monopoly` glob cannot tolerate a second
`main`. Verification is therefore reproducible runs plus a debug build that asserts the rules
that are otherwise silent when broken:

```bash
gcc -std=c99 -Wall -Wextra -pedantic *.c -o monopoly   # must be silent
gcc -std=c99 -Wall -Wextra -pedantic -g -DDEBUG *.c -o debug
./monopoly 42 > a.txt && ./monopoly 42 > b.txt && diff a.txt b.txt   # must be empty
```

`-DDEBUG` enables invariant guards on Rule 9's even building, Rule 10's houses-or-hotel
exclusion, LK 3's loan locks and the effect registry's capacity. They abort rather than warn, so
a silent run is the result you want.

## Status

✅ **Complete** — all six milestones, followed by a line-by-line audit of every §5 output
template against the spec.

A **round** is one lap of the board, not one turn each — it ends only once every solvent player
has passed GO, so a round spans roughly six turns per player. Loans and insurance policies run on
each player's own laps instead; everything else (inflation, events, regulations, depreciation)
runs on the shared round clock. Where the spec is silent or self-contradictory, the code picks
one reading and says so at the call site — the full log of those calls, D1 through D49, is in
[`docs/REQUIREMENTS.md`](docs/REQUIREMENTS.md).

| Document | What it is |
|----------|------------|
| [`docs/REQUIREMENTS.md`](docs/REQUIREMENTS.md) | Requirements checklist traced to spec rule numbers, plus every spec-gap decision and clarification override |
| [`docs/superpowers/specs/straight-to-jail-architecture-design.md`](docs/superpowers/specs/straight-to-jail-architecture-design.md) | Architecture rationale — the `Rent.csv` loader, the effect registry, the choke points, the round scheduler |
| [`docs/superpowers/plans/straight-to-jail-staged.md`](docs/superpowers/plans/straight-to-jail-staged.md) | The implementation plan — six milestones, every step compiling clean and running |
| [`docs/reference/`](docs/reference/) | Three reference notes — the C, the data structures, the economic mathematics |

### Reference material

These explain the concepts each milestone assumes, using this project's own code and numbers:

- [`01-c-language.md`](docs/reference/01-c-language.md) — multi-file compilation, enums, structs,
  pointers, `const`-correctness, **reading a file without allocating**, seeded randomness and
  modulo bias, and the bugs this project invites
- [`02-program-design.md`](docs/reference/02-program-design.md) — modelling the board, loading
  external data into fixed arrays, the effect registry, choke points, the round scheduler, state
  machines, circular queues, and verifying without a test framework
- [`03-economic-math.md`](docs/reference/03-economic-math.md) — money as `int`, rounding at the
  boundary, composing percentages, overflow headroom, compound interest, decay models, and
  expected value

<a href="/LICENSE"><img height="24px" src="https://ziadoua.github.io/m3-Markdown-Badges/badges/LicenceMIT/licencemit1.svg"></a>
