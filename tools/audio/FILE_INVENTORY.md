# Round 2 file inventory

Every reviewable file created or modified by A2 is listed below. All paths are relative to
the working-directory root. `M` means modified from the A1 baseline; `??` means newly created.
No files outside the owned paths were edited. Ogg changes are limited to four new paths and
the two original paths explained in REPORT.md. The audio attribution row alone was extended.

| Status | Path |
|---|---|
| M | `assets/ATTRIBUTION.md` |
| M | `assets/audio/SOURCES.md` |
| M | `assets/audio/animals/elephant_rumble.ogg` |
| M | `assets/audio/animals/penguin_bray.ogg` |
| M | `assets/audio/sounds.json` |
| M | `tools/audio/FILE_INVENTORY.md` |
| M | `tools/audio/README.md` |
| M | `tools/audio/REPORT.md` |
| M | `tools/audio/RESEARCH.md` |
| M | `tools/audio/VERIFICATION.md` |
| M | `tools/audio/licence-evidence.json` |
| M | `tools/audio/sources.json` |
| M | `tools/audio/verify_audio.py` |
| ?? | `assets/audio/animals/elephant_rumble_2.ogg` |
| ?? | `assets/audio/animals/elephant_rumble_3.ogg` |
| ?? | `assets/audio/animals/penguin_bray_2.ogg` |
| ?? | `assets/audio/animals/penguin_bray_3.ogg` |
| ?? | `tools/audio/AUDIT.md` |
| ?? | `tools/audio/GAINS.md` |
| ?? | `tools/audio/audit-decisions.md` |
| ?? | `tools/audio/audit_audio.py` |
| ?? | `tools/audio/audit_passages.py` |
| ?? | `tools/audio/inspect_source.py` |
| ?? | `tools/audio/round2_candidates.py` |
| ?? | `tools/audio/round2_final_search.py` |
| ?? | `tools/audio/round2_followup.py` |
| ?? | `tools/audio/round2_ledger.py` |
| ?? | `tools/audio/round2_research.py` |
| ?? | `tools/audio/round2_takes.py` |
| ?? | `tools/audio/test_audit_audio.py` |

30 reviewable paths.

## Ignored working artifacts

The following is the complete present cache inventory, including retained A1 source files,
Round 2 downloads, inspection images, request evidence, PCM snapshots, logs and reused build
intermediates. Listing a retained A1 file here does not claim it was downloaded or changed
again. All task downloads stayed under `build-probe/audio-src/`; no temporary download or
build directory outside this working tree was used. Request URLs, byte totals and source
SHA-256 values are in `round2-requests.json`, with the query ledger in RESEARCH.md.

- `build-probe/audio-src/acquire-ambience-log.txt` (2,008 bytes)
- `build-probe/audio-src/acquire-camel-log.txt` (759 bytes)
- `build-probe/audio-src/acquire-last-log.txt` (1,060 bytes)
- `build-probe/audio-src/acquire-log.txt` (7,458 bytes)
- `build-probe/audio-src/acquire-more-log.txt` (2,696 bytes)
- `build-probe/audio-src/acquire-rhino-log.txt` (303 bytes)
- `build-probe/audio-src/after-rebuild-pcm.json` (5,628 bytes)
- `build-probe/audio-src/animal-metadata-2.txt` (4,245 bytes)
- `build-probe/audio-src/animal-search-2.txt` (14,892 bytes)
- `build-probe/audio-src/animal-search-3.txt` (3,719 bytes)
- `build-probe/audio-src/animal-search.txt` (9,949 bytes)
- `build-probe/audio-src/api-05d4c0adadedf026.json` (2,264 bytes)
- `build-probe/audio-src/api-0da7575a92f2d85f.json` (2,125 bytes)
- `build-probe/audio-src/api-0e8caf76f79a64df.json` (7,973 bytes)
- `build-probe/audio-src/api-14fb16af96368266.json` (964 bytes)
- `build-probe/audio-src/api-1ee6604ed72955c5.json` (2,018 bytes)
- `build-probe/audio-src/api-26b0f8f48f215179.json` (171 bytes)
- `build-probe/audio-src/api-27b2e0380982322f.json` (4,732 bytes)
- `build-probe/audio-src/api-284759bf71df542a.json` (1,855 bytes)
- `build-probe/audio-src/api-2a7e00975cac6f96.json` (588 bytes)
- `build-probe/audio-src/api-3042143da116b203.json` (2,684 bytes)
- `build-probe/audio-src/api-3225ef477a1cff9e.json` (3,017 bytes)
- `build-probe/audio-src/api-375e3bf3e7bd96c7.json` (2,125 bytes)
- `build-probe/audio-src/api-388b9521a3f69d27.json` (9,480 bytes)
- `build-probe/audio-src/api-3bfc56a5f666a0d0.json` (2,068 bytes)
- `build-probe/audio-src/api-3e0acc5b4239c6cc.json` (661 bytes)
- `build-probe/audio-src/api-412c8eda799c3a2a.json` (4,338 bytes)
- `build-probe/audio-src/api-47a74cb9b51e58c5.json` (15,847 bytes)
- `build-probe/audio-src/api-490f7ae88cfbd80f.json` (2,474 bytes)
- `build-probe/audio-src/api-4d998e237b0e31de.json` (173 bytes)
- `build-probe/audio-src/api-5b9aa511f28f6630.json` (2,048 bytes)
- `build-probe/audio-src/api-63d651290ff03e61.json` (1,520 bytes)
- `build-probe/audio-src/api-701dbcc1e3d14f98.json` (2,892 bytes)
- `build-probe/audio-src/api-79493ce71dd2e7fc.json` (71 bytes)
- `build-probe/audio-src/api-825f8bfa6e0581c2.json` (6,674 bytes)
- `build-probe/audio-src/api-8673142b0fba2af7.json` (177 bytes)
- `build-probe/audio-src/api-8cc0c5e8cc2021c6.json` (169 bytes)
- `build-probe/audio-src/api-9b8e05da07e9ac05.json` (904 bytes)
- `build-probe/audio-src/api-9c5766be0c39ef0d.json` (2,043 bytes)
- `build-probe/audio-src/api-a0cadfe5776fb2fc.json` (2,702 bytes)
- `build-probe/audio-src/api-bee6d3267ded9c37.json` (1,560 bytes)
- `build-probe/audio-src/api-c25f91d11e322083.json` (1,950 bytes)
- `build-probe/audio-src/api-c9bfe5503809bc01.json` (7,331 bytes)
- `build-probe/audio-src/api-d10c946d689fa5d8.json` (171 bytes)
- `build-probe/audio-src/api-d6333e8d84fbfff0.json` (389 bytes)
- `build-probe/audio-src/api-da7d253cd2b682bb.json` (698 bytes)
- `build-probe/audio-src/api-df8045da2d34cd07.json` (71 bytes)
- `build-probe/audio-src/api-e603989153837a5f.json` (861 bytes)
- `build-probe/audio-src/api-ec5bdf05014a8182.json` (187 bytes)
- `build-probe/audio-src/api-ef5de1ce3a0c4bf1.json` (167 bytes)
- `build-probe/audio-src/api-f793e44c8989df50.json` (1,405 bytes)
- `build-probe/audio-src/api-fc8d6a1351fdaae1.json` (2,043 bytes)
- `build-probe/audio-src/api-fd6b9e1bd9e9383b.json` (6,988 bytes)
- `build-probe/audio-src/audio-best.ogg` (24,662 bytes)
- `build-probe/audio-src/audio-prepared.wav` (409,340 bytes)
- `build-probe/audio-src/audit-measurements.json` (23,651 bytes)
- `build-probe/audio-src/bear-metadata.txt` (2,919 bytes)
- `build-probe/audio-src/before-rebuild-pcm.json` (5,628 bytes)
- `build-probe/audio-src/camel-metadata.txt` (2,048 bytes)
- `build-probe/audio-src/elephant-rumbles.png` (621,182 bytes)
- `build-probe/audio-src/envelopes.json` (35,721 bytes)
- `build-probe/audio-src/figshare-elephant-parent.json` (5,830 bytes)
- `build-probe/audio-src/figshare-elephant-query.json` (7,106 bytes)
- `build-probe/audio-src/freesound-149233.html` (44,389 bytes)
- `build-probe/audio-src/freesound-159606.html` (43,254 bytes)
- `build-probe/audio-src/freesound-496891.html` (40,870 bytes)
- `build-probe/audio-src/freesound-516524.html` (41,222 bytes)
- `build-probe/audio-src/freesound-528067.html` (43,837 bytes)
- `build-probe/audio-src/freesound-530589.html` (50,454 bytes)
- `build-probe/audio-src/freesound-627413.html` (37,599 bytes)
- `build-probe/audio-src/freesound-746444.html` (48,042 bytes)
- `build-probe/audio-src/freesound-792852.html` (40,258 bytes)
- `build-probe/audio-src/freesound-850661.html` (38,397 bytes)
- `build-probe/audio-src/freesound-pack-5858.html` (103,572 bytes)
- `build-probe/audio-src/loop-crowd.png` (562,242 bytes)
- `build-probe/audio-src/loop-park_day.png` (517,132 bytes)
- `build-probe/audio-src/more-metadata.txt` (8,243 bytes)
- `build-probe/audio-src/rebuild-comparison.json` (968 bytes)
- `build-probe/audio-src/rebuild-reference-pcm.json` (5,438 bytes)
- `build-probe/audio-src/rebuild-result.json` (54 bytes)
- `build-probe/audio-src/round2-0093ca82e4c632cb9895.json` (286 bytes)
- `build-probe/audio-src/round2-00a192434ee376ce3f86.mov` (9,810,148 bytes)
- `build-probe/audio-src/round2-0148e503a89f1199156b.json` (41,873 bytes)
- `build-probe/audio-src/round2-01fa30107cc33d9d5392.json` (3,595 bytes)
- `build-probe/audio-src/round2-0518e85a356392e4f08c.json` (171 bytes)
- `build-probe/audio-src/round2-0772fd5539cb26f7dccf.json` (51 bytes)
- `build-probe/audio-src/round2-085beec81731a358afdb.json` (10,402 bytes)
- `build-probe/audio-src/round2-085d9b7de94367af683c.html` (89,299 bytes)
- `build-probe/audio-src/round2-0a66b383dd33749d2570.json` (19,452 bytes)
- `build-probe/audio-src/round2-0b1f3d2bbe4658f5e4bb.json` (53,287 bytes)
- `build-probe/audio-src/round2-0cba5ef313187e6da2d5.json` (2,113 bytes)
- `build-probe/audio-src/round2-0cefed25186403d9e8c1.json` (275,230 bytes)
- `build-probe/audio-src/round2-0d124a6cfb7f867ee3e3.json` (1,558 bytes)
- `build-probe/audio-src/round2-0e7031c56093b19424ac.json` (21,097 bytes)
- `build-probe/audio-src/round2-103036bd4357d32746a7.json` (169 bytes)
- `build-probe/audio-src/round2-10e48e2cc0817c9c500a.json` (4,039 bytes)
- `build-probe/audio-src/round2-10fc796d1199b734e578.xml` (5,006 bytes)
- `build-probe/audio-src/round2-110d5d246723e80a321e.json` (266,488 bytes)
- `build-probe/audio-src/round2-12108d54b82be478160e.json` (71 bytes)
- `build-probe/audio-src/round2-12988204601a7f933789.json` (23,623 bytes)
- `build-probe/audio-src/round2-12aa794d707db4a5b933.json` (12,288 bytes)
- `build-probe/audio-src/round2-13c6364e04527a7ecd8a.json` (27,902 bytes)
- `build-probe/audio-src/round2-1592f111e552d63fe9b1.json` (1,136 bytes)
- `build-probe/audio-src/round2-15b7d56245a3d30687e1.json` (51 bytes)
- `build-probe/audio-src/round2-1657b38f310038981fe9.xml` (5,006 bytes)
- `build-probe/audio-src/round2-17394e9e4a49168cbfb5.json` (22,901 bytes)
- `build-probe/audio-src/round2-17dd631259ddcf5db210.json` (64,352 bytes)
- `build-probe/audio-src/round2-180ec739ba918a2b0410.json` (35,345 bytes)
- `build-probe/audio-src/round2-185b49a358c38cd50a50.json` (40,558 bytes)
- `build-probe/audio-src/round2-1972f78826579cec66c8.json` (389 bytes)
- `build-probe/audio-src/round2-1bca1e445c4932138d0f.json` (168,860 bytes)
- `build-probe/audio-src/round2-1d3cca170c40c58289b9.json` (7,109 bytes)
- `build-probe/audio-src/round2-1f37a088298ffd34de62.json` (442 bytes)
- `build-probe/audio-src/round2-1f9d9112ba704ad614d4.json` (2,148 bytes)
- `build-probe/audio-src/round2-1fec15d984ca061f6fe0.json` (455 bytes)
- `build-probe/audio-src/round2-20dc315ca9975a051940.json` (9,480 bytes)
- `build-probe/audio-src/round2-21065553f734b018ffb3.json` (19,683 bytes)
- `build-probe/audio-src/round2-21230777efb15f11e2b5.json` (333,478 bytes)
- `build-probe/audio-src/round2-221352caff42302f9d48.webm` (7,585,397 bytes)
- `build-probe/audio-src/round2-221a210830ff85dc3bbd.html` (53,913 bytes)
- `build-probe/audio-src/round2-2344e8033065acaa2f41.json` (22,586 bytes)
- `build-probe/audio-src/round2-2430d0890f2e2105ef5f.json` (41,798 bytes)
- `build-probe/audio-src/round2-248f1b281cccdc83f37a.json` (71 bytes)
- `build-probe/audio-src/round2-272e407032e1d8015592.json` (52,511 bytes)
- `build-probe/audio-src/round2-272e812762ee5425760a.ogg` (34,745,340 bytes)
- `build-probe/audio-src/round2-27df285f62e6b63c7cf9.json` (1,177,165 bytes)
- `build-probe/audio-src/round2-2860e4a5dcc06f737845.html` (189,158 bytes)
- `build-probe/audio-src/round2-288e191e9861ca6ab88f.json` (299,871 bytes)
- `build-probe/audio-src/round2-29a36aff5fb993a167e2.ogv` (2,147,611 bytes)
- `build-probe/audio-src/round2-2a16f09688edb22ce5f7.json` (6,997 bytes)
- `build-probe/audio-src/round2-2a28ac211fabacaf6777.json` (20,271 bytes)
- `build-probe/audio-src/round2-2a81f4869a8f70be3d2e.json` (299,971 bytes)
- `build-probe/audio-src/round2-2be8e04265163d33b098.json` (2,164 bytes)
- `build-probe/audio-src/round2-2c690375e41636337107.json` (38,682 bytes)
- `build-probe/audio-src/round2-2c6e33e5e9fa88694d49.json` (34,434 bytes)
- `build-probe/audio-src/round2-2dabe36a6858d9a8e3b2.json` (40,956 bytes)
- `build-probe/audio-src/round2-2f35ef02072cc1b2335a.webm` (16,498,300 bytes)
- `build-probe/audio-src/round2-30c70af982f9140d13bc.json` (335,846 bytes)
- `build-probe/audio-src/round2-32a18edecfcd32996b32.json` (33,353 bytes)
- `build-probe/audio-src/round2-360c7763c004f89ba916.json` (5,942 bytes)
- `build-probe/audio-src/round2-3768f7dd1865d2d83994.json` (184,084 bytes)
- `build-probe/audio-src/round2-37755f5a38c0f9260e58.json` (73,708 bytes)
- `build-probe/audio-src/round2-379d156fe60eb751cc50.json` (211,923 bytes)
- `build-probe/audio-src/round2-37daed5455741bacf4e6.json` (6,199 bytes)
- `build-probe/audio-src/round2-38a1a7d542f5951bfa28.json` (461 bytes)
- `build-probe/audio-src/round2-38c5310dc252885b4d4f.json` (2,352,411 bytes)
- `build-probe/audio-src/round2-39cea466e0721cd1b658.json` (2,863 bytes)
- `build-probe/audio-src/round2-3a9c2956fb379f2d5fc4.json` (175 bytes)
- `build-probe/audio-src/round2-3b7f630fcfa466712a5b.json` (163,892 bytes)
- `build-probe/audio-src/round2-3d9e03396334ed2e356d.json` (18,182 bytes)
- `build-probe/audio-src/round2-3de26b6ffbab0d271d11.json` (8,005 bytes)
- `build-probe/audio-src/round2-3f287d8673cbf91c4e45.json` (452 bytes)
- `build-probe/audio-src/round2-40b53ae12ead28115a45.json` (36,554 bytes)
- `build-probe/audio-src/round2-4117deed7c21e1a72f87.html` (140,720 bytes)
- `build-probe/audio-src/round2-42fee28aabc2347488f1.json` (814 bytes)
- `build-probe/audio-src/round2-43657c0227f2083b0533.json` (324,942 bytes)
- `build-probe/audio-src/round2-443702f36648b96f2b1e.json` (19,233 bytes)
- `build-probe/audio-src/round2-468a2b6970d9d4381c4d.json` (1,447,019 bytes)
- `build-probe/audio-src/round2-46e632b0776c40277387.json` (173 bytes)
- `build-probe/audio-src/round2-47fd7eba40f5a2a9bf46.json` (55 bytes)
- `build-probe/audio-src/round2-49f0a9eba94d591737d8.json` (379 bytes)
- `build-probe/audio-src/round2-4abf5d80fb379657f6b2.json` (78 bytes)
- `build-probe/audio-src/round2-4af5dfbc4471d80a1470.json` (173 bytes)
- `build-probe/audio-src/round2-4bb783843f8a472a4e4a.json` (5,290 bytes)
- `build-probe/audio-src/round2-4d4612ee93ac84c6468b.json` (30,462 bytes)
- `build-probe/audio-src/round2-4d90a23dc0b4e88e52ba.json` (55 bytes)
- `build-probe/audio-src/round2-4dd2cae6f1f40ead8f9b.json` (2,047,364 bytes)
- `build-probe/audio-src/round2-4ed7cf4a226d60464847.json` (561 bytes)
- `build-probe/audio-src/round2-4ee37cde396af3fc057b.json` (493 bytes)
- `build-probe/audio-src/round2-5030fe35c22e7694e2d6.json` (286 bytes)
- `build-probe/audio-src/round2-50f904432cd05052f10b.json` (173 bytes)
- `build-probe/audio-src/round2-511c99ad4fd51ff0f2df.json` (12,498 bytes)
- `build-probe/audio-src/round2-51a5fd291d8a8ef9a7bd.json` (173 bytes)
- `build-probe/audio-src/round2-53da9912f07d513d769c.json` (1,616 bytes)
- `build-probe/audio-src/round2-55074c75a8b5eb973893.json` (40,668 bytes)
- `build-probe/audio-src/round2-55518a2cb7f18ab59c1a.json` (437,380 bytes)
- `build-probe/audio-src/round2-561542a83f1df1cb4821.json` (203,651 bytes)
- `build-probe/audio-src/round2-562b254cbe60b418282e.ogv` (13,047,420 bytes)
- `build-probe/audio-src/round2-5632129bde5482cb9c1a.json` (108,446 bytes)
- `build-probe/audio-src/round2-5643b245fe5ae4eab5f4.json` (17,121 bytes)
- `build-probe/audio-src/round2-570bfdb3f370547dda80.json` (312,413 bytes)
- `build-probe/audio-src/round2-57abc3ca1c42a335e085.json` (55 bytes)
- `build-probe/audio-src/round2-58e3fd261e2c99ed0aba.json` (222,805 bytes)
- `build-probe/audio-src/round2-59502867e87d99a7cdbc.json` (7,217 bytes)
- `build-probe/audio-src/round2-5959d96626871a56b619.json` (170,141 bytes)
- `build-probe/audio-src/round2-5ac3286c9dd84cf20f99.zip` (56 bytes)
- `build-probe/audio-src/round2-5ac4cf347b66546280d1.json` (169,689 bytes)
- `build-probe/audio-src/round2-5c23bedbf5530cae17de.json` (295,197 bytes)
- `build-probe/audio-src/round2-5e8efa00f62ddcfab2c1.json` (280 bytes)
- `build-probe/audio-src/round2-601fc03c93f114b385c7.json` (71 bytes)
- `build-probe/audio-src/round2-6039b866df417b00ed22.json` (168,468 bytes)
- `build-probe/audio-src/round2-60caadf8f195807d59cd.json` (22,351 bytes)
- `build-probe/audio-src/round2-619067662f71a3cc92e5.json` (203,433 bytes)
- `build-probe/audio-src/round2-621966733a546c894511.json` (51 bytes)
- `build-probe/audio-src/round2-624ded77171b94092892.json` (277 bytes)
- `build-probe/audio-src/round2-62b0ae5c30959261a65f.json` (292 bytes)
- `build-probe/audio-src/round2-62cfe4ffa26a4321ebdd.json` (180,689 bytes)
- `build-probe/audio-src/round2-6402970a6129a87852d8.ogg` (161,833 bytes)
- `build-probe/audio-src/round2-646038c968b010a13cc1.json` (166,793 bytes)
- `build-probe/audio-src/round2-65362d58d497fa3af55d.json` (2,520 bytes)
- `build-probe/audio-src/round2-666bb1dabb8a1f9c7c02.json` (1,450,395 bytes)
- `build-probe/audio-src/round2-680b989e655d982155d0.json` (277 bytes)
- `build-probe/audio-src/round2-69042f36937e15c39d15.json` (181 bytes)
- `build-probe/audio-src/round2-6a9f76e44a84a07305e0.json` (3,871 bytes)
- `build-probe/audio-src/round2-6b20821ea3701cf8e99b.json` (3,357 bytes)
- `build-probe/audio-src/round2-6eeac0f38e218242045f.json` (47,880 bytes)
- `build-probe/audio-src/round2-6f6f86e34f8787a20bdd.json` (51 bytes)
- `build-probe/audio-src/round2-6f85f0e78a367d053bb8.json` (588 bytes)
- `build-probe/audio-src/round2-6fb2222f181f942119b3.html` (56,996 bytes)
- `build-probe/audio-src/round2-7004f26b951bd2c3e040.json` (51 bytes)
- `build-probe/audio-src/round2-70b6536b142e88e73f4b.json` (301 bytes)
- `build-probe/audio-src/round2-731ecf508549758c5d4a.json` (1,295,564 bytes)
- `build-probe/audio-src/round2-740486709dd4c3bee38d.json` (307 bytes)
- `build-probe/audio-src/round2-74e9855d309184b4bd29.json` (6,119 bytes)
- `build-probe/audio-src/round2-755c6274316b9fdfb92e.html` (108,161 bytes)
- `build-probe/audio-src/round2-75a71aa15b323e8fd50d.json` (280 bytes)
- `build-probe/audio-src/round2-75b30d8b8a37a626d466.json` (472 bytes)
- `build-probe/audio-src/round2-75fe76676a9895c73311.json` (4,614 bytes)
- `build-probe/audio-src/round2-763a68d8aa010788d5f7.json` (48,058 bytes)
- `build-probe/audio-src/round2-76d69124165e2ca47b23.json` (50,243 bytes)
- `build-probe/audio-src/round2-76ffc94ccd036234c2fb.html` (5,550 bytes)
- `build-probe/audio-src/round2-7744affdb69316b0c131.json` (7,648 bytes)
- `build-probe/audio-src/round2-775d641c2703fae93509.json` (4,732 bytes)
- `build-probe/audio-src/round2-776f54b59a781747faea.json` (42,164 bytes)
- `build-probe/audio-src/round2-77f8b52e9a18044b6783.json` (71 bytes)
- `build-probe/audio-src/round2-783af2775c4236d2c1de.json` (20,033 bytes)
- `build-probe/audio-src/round2-7967b3c604dfaca2f302.json` (55 bytes)
- `build-probe/audio-src/round2-7afa61907d9218a8271e.json` (389 bytes)
- `build-probe/audio-src/round2-7c03dc8af0f25d377357.json` (6,839 bytes)
- `build-probe/audio-src/round2-7c40815c65f6b89ec726.mp3` (667,584 bytes)
- `build-probe/audio-src/round2-7cce81c4638ea4b201d3.json` (149,750 bytes)
- `build-probe/audio-src/round2-7d96ba1f52f5215542c6.json` (21,936 bytes)
- `build-probe/audio-src/round2-7e2e9a60ccba5d90b2c2.json` (50,370 bytes)
- `build-probe/audio-src/round2-7f35709164b51330b0fd.json` (55 bytes)
- `build-probe/audio-src/round2-8058d8bdeb660e16d71d.json` (71 bytes)
- `build-probe/audio-src/round2-80aa95e7668f7b24218c.json` (51 bytes)
- `build-probe/audio-src/round2-81f0a8a516420079926a.html` (23,879 bytes)
- `build-probe/audio-src/round2-82044f03f2097373df5f.json` (35,734 bytes)
- `build-probe/audio-src/round2-83935ab615145ee3b6b8.json` (71 bytes)
- `build-probe/audio-src/round2-839c0cdf637f5116abb9.json` (274 bytes)
- `build-probe/audio-src/round2-84d8342521758d27fe7f.json` (7,973 bytes)
- `build-probe/audio-src/round2-860a28043bbaa5f24137.json` (182 bytes)
- `build-probe/audio-src/round2-86a269380b2242828cbf.json` (71 bytes)
- `build-probe/audio-src/round2-87142de71d53d021c6b0.json` (6,025 bytes)
- `build-probe/audio-src/round2-8724e6a1ffe322d54172.json` (34,701 bytes)
- `build-probe/audio-src/round2-89863ab4ebad0ba3bb36.json` (177 bytes)
- `build-probe/audio-src/round2-89d3a67bdd4d72da53ec.json` (661 bytes)
- `build-probe/audio-src/round2-8ab1004642e254aab3cc.json` (540,548 bytes)
- `build-probe/audio-src/round2-8b0e37b134dce7298fa0.json` (124,242 bytes)
- `build-probe/audio-src/round2-8b1e93829040bdecd07d.json` (375 bytes)
- `build-probe/audio-src/round2-8c058cd72e47b335dc08.json` (360,130 bytes)
- `build-probe/audio-src/round2-8c18279b2ceafbd06cf0.json` (56,820 bytes)
- `build-probe/audio-src/round2-8cbf751883acc8e6623e.webm` (1,434,827 bytes)
- `build-probe/audio-src/round2-8dcdbdc1ea47252f592e.json` (460 bytes)
- `build-probe/audio-src/round2-8e9a7d06865ec47df5a1.json` (7,538 bytes)
- `build-probe/audio-src/round2-8ff0eec7f7e6bf8e25c4.md` (56 bytes)
- `build-probe/audio-src/round2-9021f9a0baacb8de46f6.avi` (3,364,646 bytes)
- `build-probe/audio-src/round2-90ad751975338bbf42c0.json` (19,006 bytes)
- `build-probe/audio-src/round2-913a470ef8890265007b.json` (55 bytes)
- `build-probe/audio-src/round2-91584cc50fd0ebdd2522.json` (52,686 bytes)
- `build-probe/audio-src/round2-916248e06b6ec3dde79c.json` (35,546 bytes)
- `build-probe/audio-src/round2-9186ad5ab3bf05cf8141.json` (89,163 bytes)
- `build-probe/audio-src/round2-92de9c7a3c161070ebc1.html` (56,995 bytes)
- `build-probe/audio-src/round2-9362f7b58e21b008a81a.json` (171 bytes)
- `build-probe/audio-src/round2-940c98657c72bb820e79.html` (68,155 bytes)
- `build-probe/audio-src/round2-943228888218aa311817.json` (475 bytes)
- `build-probe/audio-src/round2-96579c85ee098d3ee71d.json` (700,477 bytes)
- `build-probe/audio-src/round2-967b6990eeadbad1d257.json` (274 bytes)
- `build-probe/audio-src/round2-97163b2aa8fb4d36805c.json` (209,558 bytes)
- `build-probe/audio-src/round2-979c6424426e056697eb.json` (24,806 bytes)
- `build-probe/audio-src/round2-97dbf8ec55f11c1e0976.json` (51 bytes)
- `build-probe/audio-src/round2-9b41ab95baef2dd91460.json` (24,974 bytes)
- `build-probe/audio-src/round2-9e415716fd0b5e566161.xml` (5,002 bytes)
- `build-probe/audio-src/round2-9ff4313000af20f1e750.json` (443 bytes)
- `build-probe/audio-src/round2-a04b486014088322a402.json` (481 bytes)
- `build-probe/audio-src/round2-a12640cd5f3668d6b262.json` (10,322 bytes)
- `build-probe/audio-src/round2-a165c61d1dd020e38e3f.json` (5,830 bytes)
- `build-probe/audio-src/round2-a1de31960214c55d430e.json` (192,679 bytes)
- `build-probe/audio-src/round2-a550589701bd3136637e.m4v` (9,483,036 bytes)
- `build-probe/audio-src/round2-a58edcc0a1c2948cd306.html` (444,550 bytes)
- `build-probe/audio-src/round2-a5b46d535b9bf5ae7e5a.txt` (1,815 bytes)
- `build-probe/audio-src/round2-a6a746a7681c86906712.json` (38,716 bytes)
- `build-probe/audio-src/round2-a8c4b31a4656ce77228b.json` (18,683 bytes)
- `build-probe/audio-src/round2-aa6fa515a4e98c2058bb.html` (219,838 bytes)
- `build-probe/audio-src/round2-aa816efc854d3346ce74.json` (163,750 bytes)
- `build-probe/audio-src/round2-aab05a3e02b8dcb20d2d.json` (298 bytes)
- `build-probe/audio-src/round2-aae15031a382ae5a571b.json` (2,209 bytes)
- `build-probe/audio-src/round2-ac1d4f38c2dfcb05ed28.json` (268 bytes)
- `build-probe/audio-src/round2-ad10051f179f1677a573.json` (71 bytes)
- `build-probe/audio-src/round2-ad30d3f0422bb9bb9f38.json` (51 bytes)
- `build-probe/audio-src/round2-audit-before-trim.json` (23,608 bytes)
- `build-probe/audio-src/round2-audit-camel-inspection.png` (620,943 bytes)
- `build-probe/audio-src/round2-audit-flamingo-inspection.png` (411,216 bytes)
- `build-probe/audio-src/round2-audit-initial.log` (1,812 bytes)
- `build-probe/audio-src/round2-audit-water-inspection.png` (466,110 bytes)
- `build-probe/audio-src/round2-audit-wolf-inspection.png` (450,976 bytes)
- `build-probe/audio-src/round2-b3f4523690216a70647b.json` (36,799 bytes)
- `build-probe/audio-src/round2-b4302a024c7ab23f01b7.json` (114,630 bytes)
- `build-probe/audio-src/round2-b4a280bc29fb745afc6f.json` (59,706 bytes)
- `build-probe/audio-src/round2-b6f33040a56fa9ffda34.json` (478 bytes)
- `build-probe/audio-src/round2-b8170a78feb1ec805f56.wav` (396,944 bytes)
- `build-probe/audio-src/round2-b8eea2f71c23bf951821.wav` (5,946,752 bytes)
- `build-probe/audio-src/round2-baf8eaf520e8a57d1a94.json` (51 bytes)
- `build-probe/audio-src/round2-bb5293f0ced7d440422b.json` (481 bytes)
- `build-probe/audio-src/round2-bd08cdd6fdc1f4f29a7d.json` (181 bytes)
- `build-probe/audio-src/round2-bdd92a6bff8c7bd22d97.html` (131,041 bytes)
- `build-probe/audio-src/round2-be4bc17b4a7d78bd082b.json` (4,029 bytes)
- `build-probe/audio-src/round2-c05b2388a08d4aa1c53b.json` (55 bytes)
- `build-probe/audio-src/round2-c1a97ff2755aebae6b7e.json` (185 bytes)
- `build-probe/audio-src/round2-c21dd5ea46da702530a6.json` (67,507 bytes)
- `build-probe/audio-src/round2-c79baaa4f690f3237f36.json` (121,392 bytes)
- `build-probe/audio-src/round2-c9e71d8d6090188b2884.json` (244,711 bytes)
- `build-probe/audio-src/round2-candidate-231-inspection.png` (570,179 bytes)
- `build-probe/audio-src/round2-candidate-234-inspection.png` (527,082 bytes)
- `build-probe/audio-src/round2-candidate-235-inspection.png` (504,307 bytes)
- `build-probe/audio-src/round2-candidate-236-inspection.png` (29,250 bytes)
- `build-probe/audio-src/round2-cbe393f2839116db8a3a.html` (128,788 bytes)
- `build-probe/audio-src/round2-cbece6fcf1b2cf9fffaf.json` (169 bytes)
- `build-probe/audio-src/round2-cc716feb156992fce7a6.json` (30,958 bytes)
- `build-probe/audio-src/round2-ccfa4261334b1de7b1e6.json` (4,663 bytes)
- `build-probe/audio-src/round2-ce6d0027d79cb448d005.json` (45,299 bytes)
- `build-probe/audio-src/round2-ced60e837eb631b928fe.json` (175 bytes)
- `build-probe/audio-src/round2-d53f019cd93dcf6be3bf.json` (2,109 bytes)
- `build-probe/audio-src/round2-d5d52cc754b5fa62e7df.json` (192,364 bytes)
- `build-probe/audio-src/round2-d5ea6ae24f727fa1da34.json` (73,538 bytes)
- `build-probe/audio-src/round2-d7315c27d07b2ebcf995.json` (271 bytes)
- `build-probe/audio-src/round2-d73c1efc6c0b3c23ce55.json` (207,639 bytes)
- `build-probe/audio-src/round2-d805af16f78e70326859.json` (51 bytes)
- `build-probe/audio-src/round2-d84ea2b1dd7a4969b910.html` (211,447 bytes)
- `build-probe/audio-src/round2-d917d4f81ef4a486a521.json` (973 bytes)
- `build-probe/audio-src/round2-d9ca30382f527289e102.json` (452 bytes)
- `build-probe/audio-src/round2-db8546e2e2111bcf1d06.html` (87,103 bytes)
- `build-probe/audio-src/round2-dc25e91ea0128787a61f.json` (40,849 bytes)
- `build-probe/audio-src/round2-dda9a3b47f15279e097d.json` (16,930 bytes)
- `build-probe/audio-src/round2-de6bd67cdfc8650b8490.json` (189,629 bytes)
- `build-probe/audio-src/round2-de80957f180ebd697342.json` (167,594 bytes)
- `build-probe/audio-src/round2-de84c8299fb871c49233.json` (425 bytes)
- `build-probe/audio-src/round2-ded53ab78fbba863b871.json` (167,576 bytes)
- `build-probe/audio-src/round2-df2ce575556caf3e8a83.json` (389 bytes)
- `build-probe/audio-src/round2-e09f4c7179eb3a0f7073.m4a` (65,404 bytes)
- `build-probe/audio-src/round2-e0f2f25fdcdf0c4f61a0.json` (51 bytes)
- `build-probe/audio-src/round2-e11583ce359d01d25b4b.json` (190,005 bytes)
- `build-probe/audio-src/round2-e245fc8ed7d5ca6ce409.json` (177 bytes)
- `build-probe/audio-src/round2-e4e8d95d4329b22fa400.json` (50,258 bytes)
- `build-probe/audio-src/round2-e5fde377b663abeaecf8.json` (197,343 bytes)
- `build-probe/audio-src/round2-e6bd2452ad1c7ceb4a8c.json` (55 bytes)
- `build-probe/audio-src/round2-e8466a53f9f8e1c6d2e0.json` (227,606 bytes)
- `build-probe/audio-src/round2-e9d400c5e0c17a6a09f8.json` (6,064 bytes)
- `build-probe/audio-src/round2-eb355a58856f5034ae85.json` (33,947 bytes)
- `build-probe/audio-src/round2-ed09bc069b30b67cbf66.json` (7,106 bytes)
- `build-probe/audio-src/round2-eec53a538516f408897c.json` (991,136 bytes)
- `build-probe/audio-src/round2-ef32d22911e2f3a07b84.json` (435 bytes)
- `build-probe/audio-src/round2-ef4abf25afa7a47aa98d.json` (1,829,393 bytes)
- `build-probe/audio-src/round2-elephant-bee-inspection.png` (525,150 bytes)
- `build-probe/audio-src/round2-elephant-inspection.png` (504,944 bytes)
- `build-probe/audio-src/round2-elephant-oral-inspection.png` (503,780 bytes)
- `build-probe/audio-src/round2-f03aee22a771ce3d1f5c.json` (20,790 bytes)
- `build-probe/audio-src/round2-f2d3f927d9df33d76c3d.json` (36,607 bytes)
- `build-probe/audio-src/round2-f3b6234a5c8675ff7904.json` (37,809 bytes)
- `build-probe/audio-src/round2-f3dbcfd223fe1eedc8d5.json` (12,748 bytes)
- `build-probe/audio-src/round2-f42c1b71f49d24464b18.json` (175 bytes)
- `build-probe/audio-src/round2-f42e5e599c5cbf01ee1f.ogv` (4,474,260 bytes)
- `build-probe/audio-src/round2-f431541d646cea656a52.json` (389 bytes)
- `build-probe/audio-src/round2-f4b6e54a90a93c10afe8.json` (55 bytes)
- `build-probe/audio-src/round2-f5656d9d708a00eddcf8.json` (44,133 bytes)
- `build-probe/audio-src/round2-f56d11eab99fd125c78c.json` (207,896 bytes)
- `build-probe/audio-src/round2-f64fe49a4f0f0aa95774.json` (417,573 bytes)
- `build-probe/audio-src/round2-f6affd21cd2d6752f0f4.json` (36,591 bytes)
- `build-probe/audio-src/round2-f748d52ce39d3b5accce.json` (34,501 bytes)
- `build-probe/audio-src/round2-f9c886c31cd58787e992.json` (55 bytes)
- `build-probe/audio-src/round2-faa70278ddb81da3887d.json` (51 bytes)
- `build-probe/audio-src/round2-fbeca5fde0f59f06b6c7.json` (53,342 bytes)
- `build-probe/audio-src/round2-fddb671d95948f666886.json` (40,560 bytes)
- `build-probe/audio-src/round2-fe36004c48db4a5b478d.json` (71 bytes)
- `build-probe/audio-src/round2-fe5082b6026cc2583680.json` (173 bytes)
- `build-probe/audio-src/round2-final-audit.log` (1,812 bytes)
- `build-probe/audio-src/round2-final-build.log` (1,275 bytes)
- `build-probe/audio-src/round2-final-rebuild.log` (1,275 bytes)
- `build-probe/audio-src/round2-final-verify.log` (1,490 bytes)
- `build-probe/audio-src/round2-original-brown_bear_growl-inspection.png` (429,181 bytes)
- `build-probe/audio-src/round2-original-elephant_rumble-inspection.png` (460,043 bytes)
- `build-probe/audio-src/round2-original-giraffe_grunt-inspection.png` (396,676 bytes)
- `build-probe/audio-src/round2-original-pcm.json` (5,004 bytes)
- `build-probe/audio-src/round2-original-penguin_bray-inspection.png` (217,755 bytes)
- `build-probe/audio-src/round2-original-rhino_grunt-inspection.png` (375,410 bytes)
- `build-probe/audio-src/round2-original-zebra_bark-inspection.png` (251,087 bytes)
- `build-probe/audio-src/round2-part1-build.log` (1,161 bytes)
- `build-probe/audio-src/round2-part1-verify.log` (1,364 bytes)
- `build-probe/audio-src/round2-part2-build.log` (1,275 bytes)
- `build-probe/audio-src/round2-part2-rebuild.log` (1,275 bytes)
- `build-probe/audio-src/round2-part2-verify.log` (1,490 bytes)
- `build-probe/audio-src/round2-passage-comparison.wav` (2,629,268 bytes)
- `build-probe/audio-src/round2-passage-comparisons.json` (2,341 bytes)
- `build-probe/audio-src/round2-passage-comparisons.log` (781 bytes)
- `build-probe/audio-src/round2-penguin-display-inspection.png` (444,999 bytes)
- `build-probe/audio-src/round2-penguin-display-second-inspection.png` (416,559 bytes)
- `build-probe/audio-src/round2-penguin-inat-inspection.png` (433,060 bytes)
- `build-probe/audio-src/round2-requests.json` (137,856 bytes)
- `build-probe/audio-src/round2-trim-build.log` (167 bytes)
- `build-probe/audio-src/round2-zebra-inat-inspection.png` (344,862 bytes)
- `build-probe/audio-src/source-117e26697d2d56c8.png` (609,966 bytes)
- `build-probe/audio-src/source-117e26697d2d56c8.wav` (986,190 bytes)
- `build-probe/audio-src/source-118b0422a46dcca8.mp3` (3,760,388 bytes)
- `build-probe/audio-src/source-12ade46356687f7d.mp3` (565,048 bytes)
- `build-probe/audio-src/source-185bc61f74f5c9ae.mp3` (1,569,578 bytes)
- `build-probe/audio-src/source-185bc61f74f5c9ae.mp3.envelope.json` (7,774 bytes)
- `build-probe/audio-src/source-185bc61f74f5c9ae.png` (721,977 bytes)
- `build-probe/audio-src/source-1c54f762082a4ca8.mp3` (2,070,480 bytes)
- `build-probe/audio-src/source-1c54f762082a4ca8.png` (577,364 bytes)
- `build-probe/audio-src/source-21bd53a0d2728cad.mp3` (174,048 bytes)
- `build-probe/audio-src/source-21bd53a0d2728cad.wave.png` (3,134 bytes)
- `build-probe/audio-src/source-2fce0ab0788f4f71.ogg` (202,725 bytes)
- `build-probe/audio-src/source-2fce0ab0788f4f71.png` (430,746 bytes)
- `build-probe/audio-src/source-301ba93ea0b39203.mp3` (77,184 bytes)
- `build-probe/audio-src/source-301ba93ea0b39203.wave.png` (3,880 bytes)
- `build-probe/audio-src/source-33410662976bad18.gain-wave.png` (4,494 bytes)
- `build-probe/audio-src/source-33410662976bad18.mp3` (1,260,672 bytes)
- `build-probe/audio-src/source-33410662976bad18.wave.png` (1,912 bytes)
- `build-probe/audio-src/source-4757c712206769b7.wav` (2,366,908 bytes)
- `build-probe/audio-src/source-575049e9f63fb006.mp3` (2,112,024 bytes)
- `build-probe/audio-src/source-575049e9f63fb006.png` (739,302 bytes)
- `build-probe/audio-src/source-715517537f5b172d.gain-wave.png` (13,428 bytes)
- `build-probe/audio-src/source-715517537f5b172d.mp3` (315,993 bytes)
- `build-probe/audio-src/source-715517537f5b172d.wave.png` (3,075 bytes)
- `build-probe/audio-src/source-75124f5084b69319.mp3` (13,742,256 bytes)
- `build-probe/audio-src/source-87757bb66446976c.ogg` (77,611 bytes)
- `build-probe/audio-src/source-87757bb66446976c.png` (676,916 bytes)
- `build-probe/audio-src/source-938065f7ba410850.png` (942,917 bytes)
- `build-probe/audio-src/source-938065f7ba410850.wav` (6,301,484 bytes)
- `build-probe/audio-src/source-938065f7ba410850.wav.envelope.json` (8,507 bytes)
- `build-probe/audio-src/source-9d0aa7acfa9e31c4.mp3` (889,344 bytes)
- `build-probe/audio-src/source-9d0aa7acfa9e31c4.png` (530,432 bytes)
- `build-probe/audio-src/source-c94d53bae3a20c74.oga` (8,634 bytes)
- `build-probe/audio-src/source-c94d53bae3a20c74.png` (224,100 bytes)
- `build-probe/audio-src/source-e3919f6aefc3021e.ogg` (371,644 bytes)
- `build-probe/audio-src/source-e3919f6aefc3021e.png` (743,649 bytes)
- `build-probe/audio-src/source-elephant-control-rumbles-original.wav` (565,188 bytes)
- `build-probe/audio-src/source-f5fe42fd608da28d.png` (667,364 bytes)
- `build-probe/audio-src/source-f5fe42fd608da28d.wav` (1,363,758 bytes)
- `build-probe/audio-src/source-giraffe-grunt-original.mov` (19,833 bytes)
- `build-probe/audio-src/source-giraffe-grunt-original.png` (499,111 bytes)

Python import caches (ignored, generated while running the tools):

- `tools/audio/__pycache__/audit_audio.cpython-313.pyc`
- `tools/audio/__pycache__/build_audio.cpython-313.pyc`
- `tools/audio/__pycache__/inspect_source.cpython-313.pyc`
- `tools/audio/__pycache__/round2_research.cpython-313.pyc`

## Historical A1 inventory

The following pre-existing record is retained for context; its task-relative wording refers
to A1, not the current round.

# Files created or modified

Only `assets/ATTRIBUTION.md` existed before this task; four rows were appended.
All other non-cache files below were created for A1. Existing attribution rows are unchanged.

## Reviewable working-tree files

- `assets/ATTRIBUTION.md`
- `assets/audio/SOURCES.md`
- `assets/audio/ambience/crowd.ogg`
- `assets/audio/ambience/park_day.ogg`
- `assets/audio/ambience/water.ogg`
- `assets/audio/ambience/wind_trees.ogg`
- `assets/audio/animals/brown_bear_growl.ogg`
- `assets/audio/animals/brown_bear_huff.ogg`
- `assets/audio/animals/brown_bear_huff_2.ogg`
- `assets/audio/animals/camel_grunt.ogg`
- `assets/audio/animals/camel_grunt_2.ogg`
- `assets/audio/animals/elephant_rumble.ogg`
- `assets/audio/animals/flamingo_call.ogg`
- `assets/audio/animals/flamingo_call_2.ogg`
- `assets/audio/animals/flamingo_call_3.ogg`
- `assets/audio/animals/giraffe_grunt.ogg`
- `assets/audio/animals/lion_growl.ogg`
- `assets/audio/animals/lion_growl_2.ogg`
- `assets/audio/animals/lion_roar.ogg`
- `assets/audio/animals/lion_roar_2.ogg`
- `assets/audio/animals/meerkat_chirp.ogg`
- `assets/audio/animals/meerkat_chirp_2.ogg`
- `assets/audio/animals/meerkat_chirp_3.ogg`
- `assets/audio/animals/penguin_bray.ogg`
- `assets/audio/animals/rhino_grunt.ogg`
- `assets/audio/animals/rhino_snort.ogg`
- `assets/audio/animals/rhino_snort_2.ogg`
- `assets/audio/animals/tiger_growl.ogg`
- `assets/audio/animals/tiger_growl_2.ogg`
- `assets/audio/animals/tiger_growl_3.ogg`
- `assets/audio/animals/wolf_howl.ogg`
- `assets/audio/animals/wolf_howl_2.ogg`
- `assets/audio/animals/wolf_howl_3.ogg`
- `assets/audio/animals/zebra_bark.ogg`
- `assets/audio/animals/zebra_whinny.ogg`
- `assets/audio/footsteps/grass_1.ogg`
- `assets/audio/footsteps/grass_2.ogg`
- `assets/audio/footsteps/grass_3.ogg`
- `assets/audio/footsteps/grass_4.ogg`
- `assets/audio/footsteps/grass_5.ogg`
- `assets/audio/footsteps/gravel_1.ogg`
- `assets/audio/footsteps/gravel_2.ogg`
- `assets/audio/footsteps/gravel_3.ogg`
- `assets/audio/footsteps/gravel_4.ogg`
- `assets/audio/footsteps/gravel_5.ogg`
- `assets/audio/footsteps/paved_1.ogg`
- `assets/audio/footsteps/paved_2.ogg`
- `assets/audio/footsteps/paved_3.ogg`
- `assets/audio/footsteps/paved_4.ogg`
- `assets/audio/footsteps/paved_5.ogg`
- `assets/audio/sounds.json`
- `tools/audio/FILE_INVENTORY.md`
- `tools/audio/README.md`
- `tools/audio/REPORT.md`
- `tools/audio/RESEARCH.md`
- `tools/audio/VERIFICATION.md`
- `tools/audio/acquire_sources.py`
- `tools/audio/build_audio.py`
- `tools/audio/check_rebuild.py`
- `tools/audio/licence-evidence.json`
- `tools/audio/research.py`
- `tools/audio/sources.json`
- `tools/audio/verify_audio.py`

## Ignored downloads and intermediate files

These remain in the explicitly allowed cache, outside the delivered audio library.

- `build-probe/audio-src/acquire-ambience-log.txt`
- `build-probe/audio-src/acquire-camel-log.txt`
- `build-probe/audio-src/acquire-last-log.txt`
- `build-probe/audio-src/acquire-log.txt`
- `build-probe/audio-src/acquire-more-log.txt`
- `build-probe/audio-src/acquire-rhino-log.txt`
- `build-probe/audio-src/after-rebuild-pcm.json`
- `build-probe/audio-src/animal-metadata-2.txt`
- `build-probe/audio-src/animal-search-2.txt`
- `build-probe/audio-src/animal-search-3.txt`
- `build-probe/audio-src/animal-search.txt`
- `build-probe/audio-src/api-05d4c0adadedf026.json`
- `build-probe/audio-src/api-0da7575a92f2d85f.json`
- `build-probe/audio-src/api-0e8caf76f79a64df.json`
- `build-probe/audio-src/api-14fb16af96368266.json`
- `build-probe/audio-src/api-1ee6604ed72955c5.json`
- `build-probe/audio-src/api-26b0f8f48f215179.json`
- `build-probe/audio-src/api-27b2e0380982322f.json`
- `build-probe/audio-src/api-284759bf71df542a.json`
- `build-probe/audio-src/api-2a7e00975cac6f96.json`
- `build-probe/audio-src/api-3042143da116b203.json`
- `build-probe/audio-src/api-3225ef477a1cff9e.json`
- `build-probe/audio-src/api-375e3bf3e7bd96c7.json`
- `build-probe/audio-src/api-388b9521a3f69d27.json`
- `build-probe/audio-src/api-3bfc56a5f666a0d0.json`
- `build-probe/audio-src/api-3e0acc5b4239c6cc.json`
- `build-probe/audio-src/api-412c8eda799c3a2a.json`
- `build-probe/audio-src/api-47a74cb9b51e58c5.json`
- `build-probe/audio-src/api-490f7ae88cfbd80f.json`
- `build-probe/audio-src/api-4d998e237b0e31de.json`
- `build-probe/audio-src/api-5b9aa511f28f6630.json`
- `build-probe/audio-src/api-63d651290ff03e61.json`
- `build-probe/audio-src/api-701dbcc1e3d14f98.json`
- `build-probe/audio-src/api-79493ce71dd2e7fc.json`
- `build-probe/audio-src/api-825f8bfa6e0581c2.json`
- `build-probe/audio-src/api-8673142b0fba2af7.json`
- `build-probe/audio-src/api-8cc0c5e8cc2021c6.json`
- `build-probe/audio-src/api-9b8e05da07e9ac05.json`
- `build-probe/audio-src/api-9c5766be0c39ef0d.json`
- `build-probe/audio-src/api-a0cadfe5776fb2fc.json`
- `build-probe/audio-src/api-bee6d3267ded9c37.json`
- `build-probe/audio-src/api-c25f91d11e322083.json`
- `build-probe/audio-src/api-c9bfe5503809bc01.json`
- `build-probe/audio-src/api-d10c946d689fa5d8.json`
- `build-probe/audio-src/api-d6333e8d84fbfff0.json`
- `build-probe/audio-src/api-da7d253cd2b682bb.json`
- `build-probe/audio-src/api-df8045da2d34cd07.json`
- `build-probe/audio-src/api-e603989153837a5f.json`
- `build-probe/audio-src/api-ec5bdf05014a8182.json`
- `build-probe/audio-src/api-ef5de1ce3a0c4bf1.json`
- `build-probe/audio-src/api-f793e44c8989df50.json`
- `build-probe/audio-src/api-fc8d6a1351fdaae1.json`
- `build-probe/audio-src/api-fd6b9e1bd9e9383b.json`
- `build-probe/audio-src/audio-best.ogg`
- `build-probe/audio-src/audio-prepared.wav`
- `build-probe/audio-src/bear-metadata.txt`
- `build-probe/audio-src/before-rebuild-pcm.json`
- `build-probe/audio-src/camel-metadata.txt`
- `build-probe/audio-src/elephant-rumbles.png`
- `build-probe/audio-src/envelopes.json`
- `build-probe/audio-src/figshare-elephant-parent.json`
- `build-probe/audio-src/figshare-elephant-query.json`
- `build-probe/audio-src/freesound-149233.html`
- `build-probe/audio-src/freesound-159606.html`
- `build-probe/audio-src/freesound-496891.html`
- `build-probe/audio-src/freesound-516524.html`
- `build-probe/audio-src/freesound-528067.html`
- `build-probe/audio-src/freesound-530589.html`
- `build-probe/audio-src/freesound-627413.html`
- `build-probe/audio-src/freesound-746444.html`
- `build-probe/audio-src/freesound-792852.html`
- `build-probe/audio-src/freesound-850661.html`
- `build-probe/audio-src/freesound-pack-5858.html`
- `build-probe/audio-src/loop-crowd.png`
- `build-probe/audio-src/loop-park_day.png`
- `build-probe/audio-src/more-metadata.txt`
- `build-probe/audio-src/rebuild-comparison.json`
- `build-probe/audio-src/rebuild-reference-pcm.json`
- `build-probe/audio-src/rebuild-result.json`
- `build-probe/audio-src/source-117e26697d2d56c8.png`
- `build-probe/audio-src/source-117e26697d2d56c8.wav`
- `build-probe/audio-src/source-118b0422a46dcca8.mp3`
- `build-probe/audio-src/source-12ade46356687f7d.mp3`
- `build-probe/audio-src/source-185bc61f74f5c9ae.mp3`
- `build-probe/audio-src/source-185bc61f74f5c9ae.mp3.envelope.json`
- `build-probe/audio-src/source-185bc61f74f5c9ae.png`
- `build-probe/audio-src/source-1c54f762082a4ca8.mp3`
- `build-probe/audio-src/source-1c54f762082a4ca8.png`
- `build-probe/audio-src/source-21bd53a0d2728cad.mp3`
- `build-probe/audio-src/source-21bd53a0d2728cad.wave.png`
- `build-probe/audio-src/source-2fce0ab0788f4f71.ogg`
- `build-probe/audio-src/source-2fce0ab0788f4f71.png`
- `build-probe/audio-src/source-301ba93ea0b39203.mp3`
- `build-probe/audio-src/source-301ba93ea0b39203.wave.png`
- `build-probe/audio-src/source-33410662976bad18.gain-wave.png`
- `build-probe/audio-src/source-33410662976bad18.mp3`
- `build-probe/audio-src/source-33410662976bad18.wave.png`
- `build-probe/audio-src/source-4757c712206769b7.wav`
- `build-probe/audio-src/source-575049e9f63fb006.mp3`
- `build-probe/audio-src/source-575049e9f63fb006.png`
- `build-probe/audio-src/source-715517537f5b172d.gain-wave.png`
- `build-probe/audio-src/source-715517537f5b172d.mp3`
- `build-probe/audio-src/source-715517537f5b172d.wave.png`
- `build-probe/audio-src/source-75124f5084b69319.mp3`
- `build-probe/audio-src/source-87757bb66446976c.ogg`
- `build-probe/audio-src/source-87757bb66446976c.png`
- `build-probe/audio-src/source-938065f7ba410850.png`
- `build-probe/audio-src/source-938065f7ba410850.wav`
- `build-probe/audio-src/source-938065f7ba410850.wav.envelope.json`
- `build-probe/audio-src/source-9d0aa7acfa9e31c4.mp3`
- `build-probe/audio-src/source-9d0aa7acfa9e31c4.png`
- `build-probe/audio-src/source-c94d53bae3a20c74.oga`
- `build-probe/audio-src/source-c94d53bae3a20c74.png`
- `build-probe/audio-src/source-e3919f6aefc3021e.ogg`
- `build-probe/audio-src/source-e3919f6aefc3021e.png`
- `build-probe/audio-src/source-elephant-control-rumbles-original.wav`
- `build-probe/audio-src/source-f5fe42fd608da28d.png`
- `build-probe/audio-src/source-f5fe42fd608da28d.wav`
- `build-probe/audio-src/source-giraffe-grunt-original.mov`
- `build-probe/audio-src/source-giraffe-grunt-original.png`
