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

**MONOPOLY-LK** — a fully autonomous, Sri-Lanka-themed Monopoly economic simulation in C. Four AI players with distinct financial personalities battle across a 40-square Colombo-to-Jaffna board through loans, insurance, inflation, disasters, and market swings. 

Take-home assignment for *SCS 1301 – Data Structures and Program Design using C* (University of Colombo School of Computing). The full ruleset lives in [`docs/Assignment_1_unlocked.pdf`](docs/Assignment_1_unlocked.pdf).

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

## Build

```bash
gcc *.c -o monopoly     # the canonical grading build (spec §4)
make                    # the same build, via the Makefile
make straight_to_jail   # warnings on: -std=c99 -Wall -Wextra -pedantic
make debug              # -g -DDEBUG, written to monopoly
make clean
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

## Verifying a build

There is no test binary — the mandated `gcc *.c -o monopoly` glob cannot tolerate a second
`main`. Verification is therefore reproducible runs plus a debug build that asserts the rules
that are otherwise silent when broken:

```bash
gcc -std=c99 -Wall -Wextra -pedantic *.c -o monopoly   # must be silent
gcc -std=c99 -Wall -Wextra -pedantic -g -DDEBUG *.c -o debug
./monopoly 42 > a.txt && ./monopoly 42 > b.txt && diff a.txt b.txt   # must be empty
```

## Debugging in Zed

[`.zed/debug.json`](.zed/debug.json) defines a **Debug monopoly (MSYS2 GDB)** launch
configuration. It runs `make debug` and then starts `monopoly.exe` under the UCRT64 `gdb`. The
`make` and `gdb` paths should point to an MSYS2 install; change the `build.command` and `gdb_path` entries.

## Docs

| Document | What it is |
|----------|------------|
| [`docs/REQUIREMENTS.md`](docs/REQUIREMENTS.md) | Requirements checklist traced to spec rule numbers, plus every spec-gap decision and clarification override |
| [`docs/superpowers/specs/straight-to-jail-architecture-design.md`](docs/superpowers/specs/straight-to-jail-architecture-design.md) | Architecture rationale — the `Rent.csv` loader, the effect registry, the choke points, the round scheduler |
| [`docs/superpowers/plans/straight-to-jail-staged.md`](docs/superpowers/plans/straight-to-jail-staged.md) | The implementation plan — six milestones, every step compiling clean and running |
| [`docs/reference/`](docs/reference/) | Three reference notes — the C, the data structures, the economic mathematics |
| [`docs/compliance_gpt.md`](docs/compliance_gpt.md), [`docs/compliance_grok.md`](docs/compliance_grok.md) | Independent conformance reviews against the assignment PDF, with a requirements checklist and severity-ranked findings |
| [`docs/stat_report.pdf`](docs/stat_report.pdf) | Statistical report on 1,000,000 seeded games |
| [`docs/Assignment_1_unlocked.pdf`](docs/Assignment_1_unlocked.pdf) | The assignment specification |

### Results from 1,000,000 games

Seeds 1 to 1,000,000 were run to completion ([`docs/stat_report.pdf`](docs/stat_report.pdf)):

| Strategy | Win rate |
|----------|----------|
| Aggressive Investor | 43.52% |
| Risk Taker | 27.68% |
| Opportunistic Trader | 17.93% |
| Conservative Banker | 10.87% |

Most games end by elimination, at a median of 43 rounds. Only 5 of the million reached the
500-round cap. In 7.08% of games the last solvent player finishes with negative net worth,
because compound interest on an unrepaid loan grows faster than the estate. In 1,261 games the
loan balance hits the `int32` ceiling. The report's harness (`stats/`) is not in this
repository.

<a href="/LICENSE"><img height="24px" src="https://ziadoua.github.io/m3-Markdown-Badges/badges/LicenceMIT/licencemit1.svg"></a>