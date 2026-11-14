# Financial model tutorial: source contract and replay checklist

Source: [Create a Financial Model in 10 Minutes!](https://www.youtube.com/watch?v=DBNVhmI6WtQ), The Financial Controller, published October 27, 2025. Player duration: **10:19**. Source inspected November 14, 2026 using the live YouTube transcript and paused video frames in the Codex browser.

This file records the source evidence and required replay. It is **not a claim that TigerSheets has passed these steps**. Implementation verification belongs in the acceptance-test results. Timestamps identify source checkpoints, sometimes a few seconds after the action begins.

## Source corrections that affect acceptance

1. Although the narration at 1:20 says September 2026, the original date-fill sequence actually extends to **R3 = 12/31/26** by approximately 1:23. At 1:21, O3:Q3 is selected, containing September through November 2026. The next inspected frame has O3:R3 selected and December populated. Subsequent whole-sheet views, including 4:11, confirm forecasts in **D:R, fifteen months**.
2. The profitable scenario near 9:08 is **20% revenue / 15% COGS / 15% Opex**. It first becomes positive in April 2026, J9, displaying **$101,617**.
3. The presenter changes the revenue dropdown once more at approximately 9:50. The **last demonstrated state at 9:55 is 15% / 15% / 15%**, and net income becomes increasingly negative. An acceptance run ending at the profitable scenario omits a real video action.
4. Early bright yellow rectangles and cyan output rectangles, with drop shadows and animated partial coverage of cells, are video annotations. The actual yellow cell fill is applied to B11:C14 near 9:43–9:48.

## Starting workbook

The presenter starts with an existing actual-data sheet. The initial tab name is not legible in the inspected cropped frames; use the project's existing main model sheet and create a second sheet named **Input**.

| Cell | Value / formula | Display observed |
| --- | --- | --- |
| B3 | Category | Category |
| C2 | Actual | Actual, horizontally centered |
| C3 | September 30, 2025 date | 9/30/25; formula bar shows 9/30/2025 |
| B4 | Revenue | Revenue |
| C4 | 4000000 | $4,000,000 |
| B5 | Cost of Sales | Cost of Sales |
| C5 | 2500000 | $2,500,000 |
| B6 | Gross Profit | Gross Profit |
| C6 | =C4-C5 | $1,500,000; formula directly inspected at 6:58 |
| B8 | Opex | Opex |
| C8 | 2850000 | $2,850,000 |
| B9 | Net Income | Net Income |
| C9 | =C6-C8 | -$1,350,000; formula directly inspected at 1:01 and 7:33 |
| B11 | Assumptions | Assumptions |
| B12 | Revenue Growth | Revenue Growth |
| B13 | Cogs Growth | Cogs Growth |
| B14 | Opex Growth | Opex Growth |

Rows 1, 7, and 10 provide blank spacing. Initial assumption cells C12:C14 are blank. Main-sheet font shown in the toolbar is **Arial (Body), 10 pt**. Labels B11:B14 are light gray at 1:01 and black in later inspected frames. The exact operation changing their font color is not established by the sampled frames.

## Step-by-step replay

Each row describes a user action or a distinct observable checkpoint. Formula filling must use the real fill-handle behavior, including horizontal scrolling when the destination is off screen. Editing formulas must preserve relative and mixed-reference semantics.

| # | Source time | Action to replay | Required observable result / source evidence |
| --- | --- | --- | --- |
| 1 | 0:46–1:01 | Inspect starting actual values and select C9. | Values above; formula bar `=C6-C8`. |
| 2 | 1:02–1:16 | Select D3, enter `=EOMONTH(C3,1)`, commit. | D3 displays 10/31/25; exact formula visible at 1:16. |
| 3 | 1:16–1:23 | Drag the month formula right and extend the range to R3. | E3 11/30/25, F3 12/31/25, G3 1/31/26, H3 2/28/26, I3 3/31/26, J3 4/30/26, K3 5/31/26, L3 6/30/26, M3 7/31/26, N3 8/31/26, O3 9/30/26, P3 10/31/26, Q3 11/30/26, R3 12/31/26. |
| 4 | 1:24–1:33 | Enter Forecast in D2; fill across to R2. | C2 remains Actual; every D2:R2 cell says Forecast. Whole-sheet view at 4:11 confirms endpoint R. |
| 5 | 1:50–1:59 | Add a sheet; rename it Input. | A separate Input worksheet is usable without losing main-sheet state. |
| 6 | 1:59–2:16 | Copy main-sheet B12:B14. Select Input!B2; open Paste Special; choose Transpose; confirm. | Input!B2 Revenue Growth, C2 Cogs Growth, D2 Opex Growth. Directly inspected at 2:16. |
| 7 | 2:19–2:21 | Enter zero percent in Input!B3. | B3 displays 0.00%; formula bar displays 0%. |
| 8 | 2:21–2:24 | Copy B3; select B4:B7; paste. | Percentage format and zero value copied down. At 2:24 B3 has copy marching ants and B4:B7 is selected. At 2:29 B6:B7 still display 0.00%. |
| 9 | 2:24–2:29 | Replace B4 with 10% and B5 with 15%. | B4 10.00%; B5 is visibly being edited as `15%` at 2:29. |
| 10 | 2:31–2:38 | Replace B6 with 20% and B7 with 25%. | Five revenue scenarios: 0.00%, 10.00%, 15.00%, 20.00%, 25.00%. |
| 11 | 2:39–2:47 | Copy the five revenue scenarios to C3:C7 and D3:D7. | Same values and percentage formatting in all three scenario columns. |
| 12 | 2:47–2:54 | Change Input!D3 from 0% to 5%. | Opex scenarios start at 5.00%. |
| 13 | 3:02–3:11 | Change Input!B7 to -10%; copy it to C7. | Both B7 and C7 display -10.00%; B7 still has marching ants at 3:11. |
| 14 | 3:10–3:16 | Change Input!D7 from 25% to -5%. | Final Input table below; directly inspected at 3:16. |
| 15 | 3:34–3:56 | Return to main sheet, select C12, open Data Validation, choose List, focus Source, switch to Input, select B3:B7. | Source field directly shows `=Input!$B$3:$B$7`; Ignore blank and In-cell dropdown checked. |
| 16 | 3:56–4:01 | Confirm validation. Return to C12 and open the dropdown. | Menu shows 0.00%, 10.00%, 15.00%, 20.00%, -10.00%. At 4:01, 20.00% is hovered, but C12 remains blank. |
| 17 | 4:04–4:11 | Select C13 and repeat List validation using Input C3:C7. | C13 has a dropdown at 4:11. Expected source `=Input!$C$3:$C$7`; exact dialog text was not captured in the sampled frames. |
| 18 | 4:11–4:16 | Select C14 and repeat List validation using Input D3:D7. | Three dropdowns are described in the transcript; C12:C14 remain blank at 4:16. Expected source `=Input!$D$3:$D$7`; exact dialog text was not captured in the sampled frames. |
| 19 | 4:27–4:48 | Select D4. Build `=C4*(1+C12)` by typing operators and clicking referenced cells. Commit. | Exact in-cell edit and formula-bar text visible at 4:48. C4 and C12 receive distinct reference outlines. Blank C12 acts as zero; D4 is $4,000,000 after commit. |
| 20 | 4:53–4:58 | Open C12 dropdown and choose 10%. | C12 displays 10%; D4 recalculates to $4,400,000. |
| 21 | 5:00–5:03 | Choose 15% in C12. | D4 recalculates to $4,600,000. |
| 22 | 5:03–5:18 | Drag D4 right through R4, with the initially unfixed assumption reference. | Every D4:R4 value is $4,600,000. J4 formula bar at 5:18 is `=I4*(1+I12)`. Reproducing this intentional tutorial mistake matters. |
| 23 | 5:07–5:18 | Autofit columns that do not display their values. | Complete currency figures display across D:R at 5:18. Exact column-header selection gesture is not established by sampled frames. |
| 24 | 5:15–5:28 | Double-click F4 to inspect its formula. | Exact F4 formula `=E4*(1+E12)`; E4 blue reference outline and E12 red outline show the displaced assumption reference. |
| 25 | 5:31–5:53 | Edit the first revenue forecast D4, adding `$` before C in the growth reference. | Exact corrected formula `=C4*(1+$C12)`; only the column is absolute. |
| 26 | 5:56–6:03 | Commit and drag corrected D4 across through R4. | Compound growth: D4 $4,600,000; E4 $5,290,000; F4 $6,083,500; G4 $6,996,025; H4 $8,045,429; I4 $9,252,243; J4 $10,640,080; K4 $12,236,091; L4 $14,071,505; M4 $16,182,231; N4 $18,609,566; O4 $21,401,000; P4 $24,611,150; Q4 $28,302,823; R4 $32,548,247. Whole row inspected at 6:03. |
| 27 | 6:05–6:13 | Change C12 to 10%. | Compound revenue recalculates throughout; D4 $4,400,000, E4 $4,840,000, F4 $5,324,000, G4 $5,856,400. G4 formula `=F4*(1+$C12)`. |
| 28 | 6:13–6:23 | Choose 10% in C13, then use the percentage-format toolbar action. | C13 displays 10%, not decimal 0.1. |
| 29 | 6:19–6:38 | Choose 5% in C14, then apply percentage formatting. | C14 displays 5%. C12/C13/C14 are now 10%/10%/5%. |
| 30 | 6:26–6:38 | Drag D4 down one row to D5; double-click to inspect. | Formula adjusts to `=C5*(1+$C13)`; C5 and C13 outlined; row reference moves, fixed column remains C. |
| 31 | 6:40–6:48 | Commit and fill D5 through R5. | COGS compounds at 10%; D5 $2,750,000, E5 $3,025,000, F5 $3,327,500, G5 $3,660,250. |
| 32 | 6:54–6:58 | Select C6, inspect `=C4-C5`, then drag it through R6. | D6 $1,650,000, E6 $1,815,000, F6 $1,996,500, G6 $2,196,150. |
| 33 | 7:02–7:23 | Select D8. Enter `=C8*(1+$C14)` using cell references; commit. | Exact in-progress formula text visible at 7:23, before the final closing parenthesis. D8 $2,992,500 after commit. |
| 34 | 7:25–7:31 | Drag D8 through R8. | Opex compounds at 5%; E8 $3,142,125, F8 $3,299,231, G8 $3,464,193. |
| 35 | 7:31–7:33 | Select C9, inspect `=C6-C8`, then drag through R9. | Entire forecast calculates; D9 -$1,342,500, E9 -$1,327,125, F9 -$1,302,731, G9 -$1,268,043, H9 -$1,221,637, I9 -$1,161,931, J9 -$1,087,161 at 7:43. |
| 36 | 7:54–7:58 | Choose C12 = 15%. | C13 stays 10%, C14 stays 5%. D9 -$1,142,500; E9 -$877,125; F9 -$543,231; G9 -$128,418; H9 $381,751. |
| 37 | 8:02 | Keep C13 at 10%, as narrated. | No mathematical change from previous COGS assumption. |
| 38 | 8:02–8:08 | Choose C14 = 10%. | Scenario 15%/10%/10%. D9 -$1,285,000; E9 -$1,183,500; F9 -$1,037,350; G9 -$836,910; H9 -$570,800; I9 -$225,608; J9 $214,443; K9 $767,891. |
| 39 | 8:57–9:03 | Choose C12 = 20%, then C13 = 15%. | Intermediate scenario 20%/15%/10%; D9 -$1,210,000, E9 -$994,750, F9 -$683,538, G9 -$250,801, H9 $334,934. |
| 40 | 9:04–9:08 | Choose C14 = 15%. | Scenario 20%/15%/15%; D9 -$1,352,500; E9 -$1,315,375; F9 -$1,224,681; G9 -$1,062,783; H9 -$807,481; I9 -$430,939; J9 $101,617; K9 $833,496. |
| 41 | 9:11–9:18 | Inspect the transition from loss in March to profit in April. | J3 4/30/26; J9 $101,617. The video says break-even but displayed income is positive, not exactly zero. |
| 42 | 9:26–9:28 | Select C3:R3 and click Bold. | Only dates are selected; B3 Category is excluded. Date labels become bold Arial 10. |
| 43 | 9:28–9:33 | Open fill-color palette; choose light blue. | C3:R3 has a pale blue fill. Exact RGB or theme color tooltip was not captured; do not describe an inferred RGB as source-verified. |
| 44 | 9:32–9:40 | Select C2:R2; click Bold; change font size from 10 to 9. | Toolbar shows Arial (Body), size 9, Bold active at 9:40; label text remains centered. |
| 45 | 9:40–9:43 | Select B11:C14, four rows by two columns. | Name box shows 4R x 2C at 9:43. B11 Assumptions is bold; the other assumption labels are normal weight. |
| 46 | 9:43–9:48 | Use fill-color palette to choose yellow. | Actual B11:C14 cell fill is bright yellow. |
| 47 | 9:48–9:50 | Open C12 dropdown again. | Source-formatted options still display two decimal places; 15.00% is hovered at 9:50 while C12 still says 20%. |
| 48 | 9:50–9:55 | Select 15.00% in C12, then click away to D17. | Last demonstrated scenario 15%/15%/15%; full dependent model recalculates. D9 -$1,552,500, E9 -$1,785,375, F9 -$2,053,181, G9 -$2,361,158, H9 -$2,715,332, I9 -$3,122,632, J9 -$3,591,027, K9 -$4,129,681. |
| 49 | 9:55–10:19 | Inspect final model while the presenter closes the tutorial. | No additional modeling operation described in the closing transcript. Downloading the advertised template is not needed to reproduce the spreadsheet behavior. |

## Final Input values

| Row | B: Revenue Growth | C: Cogs Growth | D: Opex Growth |
| --- | --- | --- | --- |
| 3 | 0.00% | 0.00% | 5.00% |
| 4 | 10.00% | 10.00% | 10.00% |
| 5 | 15.00% | 15.00% | 15.00% |
| 6 | 20.00% | 20.00% | 20.00% |
| 7 | -10.00% | -10.00% | -5.00% |

The B5 edit visibly contains `15%`. The recording does not disclose physical keyboard events, so it cannot establish whether `%` was typed explicitly or retained by Excel when replacing a value in an already percentage-formatted cell. Replaying an explicit percentage string and the equivalent edit in a formatted cell should yield the same numeric fraction. The mechanism producing B3's initial two-decimal percentage format is also not established by the sampled frames.

## Final displayed results

The table below is an independently calculated oracle from the verified formulas and final 15%/15%/15% assumptions. Whole-dollar display uses half-away-from-zero rounding; underlying cells retain full precision. **D:K are directly legible in the inspected 9:55 frame. L is at the right crop edge, with some entries partly cropped. M:R are computed expectations, not a claim that those values were legible in that final source frame.**

| Column / date | Revenue, row 4 | COGS, row 5 | Gross profit, row 6 | Opex, row 8 | Net income, row 9 |
| --- | ---: | ---: | ---: | ---: | ---: |
| D 10/31/25 | 4,600,000 | 2,875,000 | 1,725,000 | 3,277,500 | -1,552,500 |
| E 11/30/25 | 5,290,000 | 3,306,250 | 1,983,750 | 3,769,125 | -1,785,375 |
| F 12/31/25 | 6,083,500 | 3,802,188 | 2,281,313 | 4,334,494 | -2,053,181 |
| G 1/31/26 | 6,996,025 | 4,372,516 | 2,623,509 | 4,984,668 | -2,361,158 |
| H 2/28/26 | 8,045,429 | 5,028,393 | 3,017,036 | 5,732,368 | -2,715,332 |
| I 3/31/26 | 9,252,243 | 5,782,652 | 3,469,591 | 6,592,223 | -3,122,632 |
| J 4/30/26 | 10,640,080 | 6,650,050 | 3,990,030 | 7,581,057 | -3,591,027 |
| K 5/31/26 | 12,236,091 | 7,647,557 | 4,588,534 | 8,718,215 | -4,129,681 |
| L 6/30/26 | 14,071,505 | 8,794,691 | 5,276,814 | 10,025,947 | -4,749,133 |
| M 7/31/26 | 16,182,231 | 10,113,894 | 6,068,337 | 11,529,840 | -5,461,503 |
| N 8/31/26 | 18,609,566 | 11,630,978 | 6,978,587 | 13,259,315 | -6,280,728 |
| O 9/30/26 | 21,401,000 | 13,375,625 | 8,025,375 | 15,248,213 | -7,222,838 |
| P 10/31/26 | 24,611,150 | 15,381,969 | 9,229,181 | 17,535,445 | -8,306,263 |
| Q 11/30/26 | 28,302,823 | 17,689,264 | 10,613,559 | 20,165,761 | -9,552,203 |
| R 12/31/26 | 32,548,247 | 20,342,654 | 12,205,592 | 23,190,626 | -10,985,033 |

## Formatting observations and limits

- Dates use numeric month/day/two-digit-year display (`m/d/yy`), not month-name labels.
- Main assumption cells display whole percentages; Input scenarios and dropdown options display two decimals.
- Source revenue and COGS forecast cells show accounting-style spacing between `$` and the number; source gross-profit, Opex, and net-income rows use adjacent currency symbols. Initial actual values use adjacent currency symbols.
- Negative currency uses a leading minus before the dollar symbol, e.g. `-$1,552,500`, rather than parentheses.
- Exact pixel matching to Excel's video is not attainable from a different native toolkit merely by matching spreadsheet values. Font family/size, bold state, fill colors, alignment, number formats, and readable column widths are independently verifiable behaviors.
- The inspected browser screenshots were emitted as evidence in the research tool outputs. The browser API supplied no documented screenshot-to-file destination; no reference-image files are claimed here.
- Transcript extraction via the dedicated export API returned unavailable; the visible Show transcript panel did expose the complete caption sequence. Web fetch of the watch URL returned cache miss; the browser player and visible transcript were the successful evidence sources.
