# Gameplay update review

Branches: `feat/gameplay-development-updates` in the engine and Unity repositories. The engine branch includes existing traits/trades and staff/settings prerequisites that the current UI already calls. Merge the Unity PR first, then the engine PR carrying its submodule pointer. No automatic merge is enabled.

## Behavior

- Team links open club pages from the sidebar, match scoreboard, roster headings, schedule opponents, playoff participants, player profile, draft results and trades. Match simulation pauses while inspecting a club.
- Camp remains a separate offseason plan. Season training supports 0-7 sessions per week, light/normal/heavy intensity, a team focus and individual player focuses. Rest restores readiness; heavy workloads reduce readiness, slow gains at high fatigue, and increase injury risk. Young players with potential gain more from combining practice and match experience.
- Mentors must be at least 28 and older than their mentee. Same/adjacent positions qualify. A veteran aged 34+ with Leadership 85+ may mentor across positions. Invalid, self and departed-player pairs confer no benefit.
- Open leagues expose each detailed profile rating at its displayed 25-99 scale. Overall and simulation attribute groups follow the edited ratings. Contract and trait editing remain available. Detailed edits, individual training and readiness are stored in existing career saves with defaults for older saves.
- Live rosters scroll through all players, with players on court first and highlighted green. Squad lists place starters first in PG-to-C order; replacing a starter immediately refreshes the ordering.
- Courts have home-team color/name and floor variation. Three-point arcs join baseline corner lines at the 22-foot offset, using the 23-foot-9-inch radius.
- Fast breaks attack the rim; runners continue into finishes rather than half-court spacing. Assisted dunks can show alley-oop commentary. Outcomes still allow realistic misses, turnovers and defensive stops.
- Shouts offer more shooting, inside play, tighter defense, harder defense, ball movement and play-through-player. Offensive shouts expire after two own attacks; defensive shouts after two opposing attacks. Permanent tactics remain unchanged. Harder defense increases foul risk.
- Scouting supports guards (PG/SG), forwards (SF/PF) and bigs (PF/C), with offensive/all-around/defensive assignments. The class exists during the season. Reports arrive every two weeks, narrow estimate ranges over the year and prioritize team fit and projected draft range.
- News includes player development, injuries/returns, contracts, scout updates and league results. Game news opens a box score; player/scout news opens the relevant view. League boxes are captured immediately after games and kept for three seasons.
- Import opens a system picker on macOS/Windows and a document provider on Android/iOS. Select `teams.csv` and `players.csv` together; desktop users may select either from their shared folder. Selected files are copied into a private league-import folder. Info explains required headers/ranges and template generation remains available.
- Holding Import continuously for **more than 15 seconds** reveals the NBA tester option. A short click retains normal file import. This is a discoverability gesture, not access control. NBA selection copies the bundled database into the new career.

## Validation and release gates

Automated: native build/test suite; C# compiler checks against Unity's bundled .NET Standard 2.1 references for Editor, macOS, Windows, Android and iOS; universal macOS plugin build and exported API checks.

Unity's batch editor could not complete licensing initialization on this machine. Therefore rendered layouts, pointer/touch behavior and full player builds remain unverified. Before release:

- [ ] Run the actual training, mentoring, editing, scouting and match flows in Unity and inspect desktop/mobile layouts.
- [ ] Confirm every displayed team reference reaches the intended club, including secondary text and modal views.
- [ ] Play fast breaks and exercise all shouts, including substitution of a selected play-through target.
- [ ] Verify short import clicks, cancellation, permission denial, leaving the button during a hold, 15-second boundary, and tester NBA selection.
- [ ] Test system file pickers on macOS, Windows, Android and a signed iPhone build.
- [ ] Rebuild Windows `BMCore.dll`, Android `libBMCore.so` and iOS `libBMCore.a` from this engine branch before packaging. Their committed binaries predate the new APIs; only the macOS plugin is refreshed here.
- [ ] Run device smoke tests and release-sign/notarize platform artifacts as appropriate.

Cloud/local switching, account recovery and login UI redesign are intentionally deferred.

## References

- [NBA court dimensions](https://official.nba.com/rule-no-1-court-dimensions-equipment/)
- [Native File Picker](https://github.com/yasirkula/UnityNativeFilePicker), vendored at `f2347e789ccfe35655d2955ca7b4b21123143efd` under its included MIT license. Desktop selection uses OS dialogs; the mobile plugin supports Android/iOS only.
