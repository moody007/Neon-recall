# Design rationale

## Competitive tension

Both players see the same maze but approach the central vault from opposite lower corners. The reveal is shared, so success involves spatial memory and reading which route the rival is likely to take. First place gets the major score and first draft; the trailing player still gets a separate counter-pick to prevent one early win from deciding a long match.

## Difficulty curve

- Reveal duration falls from roughly 5.8 seconds toward a 2.2-second floor.
- Blackout duration falls gradually but retains an 18-second floor.
- Poison count grows by two per round where suitable off-route floor cells exist.
- Additional maze loops increase later-round route ambiguity.
- The actual maze, safe path, and poison layout regenerate every round.

Difficulty therefore comes from denser decisions and reduced observation—not invisible random damage or impossible routes.

## Readability and fairness invariants

- Movement cannot occur during the reveal.
- The center vault and both spawns are connected.
- No solution route is displayed; players must identify and memorize one.
- Poison is concentrated on useful corridors/junctions, visible during memorization, and relocated before the next memory frame.
- A greedy reachability check rejects any poison placement that would eliminate the final poison-free path from either player's current position or respawn point.
- Triggered poison disappears immediately; only the affected player returns to spawn.
- Wall collisions do not cost health.
- Poison consequences are predictable.
- Players have identical base capabilities.
- A second-place player gets five seconds after the winner reaches the vault.

## Freshness

The procedural maze is the main source of replay variety. Buff combinations add strategic identities: a speed build, a poison-resistant build, a memory-assistance build, or a high-health conservative build. Match-length-dependent draft size changes how reliably a player can assemble a preferred build.

Difficulty also changes maze decision density. Early rounds retain a more tree-like structure, while later rounds open a guaranteed, increasing number of scored connectors. These connectors prioritize existing corridor intersections, producing more cells with three or four exits and more believable alternate directions during blackout.
