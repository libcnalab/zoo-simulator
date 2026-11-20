# Construction catalog

Costs are purchase prices in dollars. Fence prices are per two metre edge; path and terrain prices are per tile; food prices are per kilogram. Daily upkeep is shown where applicable.

## Fences

| ID | Name | Height | Strength | Visibility | Cost |
|---|---|---:|---:|---:|---:|
| wooden_rail | Wooden rail | 1.2 m | 1 | 0.75 | $45 |
| wire_mesh | Wire mesh | 2.0 m | 2 | 0.80 | $90 |
| tall_mesh | Tall mesh | 3.5 m | 3 | 0.82 | $160 |
| steel_bars | Steel bars | 4.0 m | 4 | 0.70 | $280 |
| glass_wall | Glass wall | 3.5 m | 4 | 1.00 | $650 |
| concrete_wall | Concrete wall | 3.5 m | 5 | 0.00 | $210 |
| cable_barrier | Cable barrier | 2.4 m | 5 | 0.90 | $390 |
| hedge | Clipped hedge | 1.0 m | 1 | 0.25 | $65 |
| high_cable_barrier | High cable barrier | 5.0 m | 5 | 0.80 | $760 |
| high_glass_wall | High glass wall | 4.5 m | 4 | 1.00 | $940 |
| heavy_mesh | Heavy welded mesh | 4.5 m | 4 | 0.80 | $230 |
| stand_off_rail | Stand-off rail | 1.1 m | 1 | 1.00 | $50 |
| paddock_rail | Timber paddock fence | 2.4 m | 3 | 0.90 | $120 |
| tall_cable_fence | Tall cable fence | 3.0 m | 3 | 0.92 | $240 |
| dry_moat | Dry moat and ha-ha | 2.4 m | 5 | 1.00 | $520 |
| mesh_backed_hedge | Mesh-backed wildlife hedge | 1.8 m | 2 | 0.30 | $140 |

Fence kinds 1–10 are unchanged; `heavy_mesh` is 11, `stand_off_rail` is 12,
`paddock_rail` is 13, `tall_cable_fence` is 14, `dry_moat` is 15 and
`mesh_backed_hedge` is 16.
The existing wooden rail remains unchanged (visibility 0.75). The new public rail
and the hidden retaining wall of a graded dry moat have visibility 1.0 so they do not
unnecessarily obscure an already screened view. The wildlife hedge is taller and stronger than
the decorative clipped hedge because welded mesh and a buried edge sit inside its foliage.

## Paths and terrain

| Path ID | Name | Cost/tile | Speed | Appeal |
|---|---|---:|---:|---:|
| asphalt | Asphalt path | $35 | 1.15 | 0.05 |
| gravel | Gravel path | $18 | 0.95 | 0.10 |
| cobblestone | Cobblestone path | $48 | 0.90 | 0.25 |
| boardwalk | Boardwalk | $55 | 1.05 | 0.20 |

| Terrain ID | Name | Cost/tile |
|---|---|---:|
| grass | Grass | $8 |
| savanna | Savanna grass | $10 |
| dirt | Bare earth | $3 |
| sand | Sand | $7 |
| rock | Rocky ground | $18 |
| snow | Snow | $14 |
| forest_floor | Forest floor | $12 |
| mud | Mud | $4 |

## Food

| ID | Name | Cost/kg | Shelf life |
|---|---|---:|---:|
| meat | Meat | $2.40 | 1 day |
| fish | Fish | $2.10 | 1 day |
| hay | Hay | $0.22 | 14 days |
| browse | Leafy browse | $0.55 | 3 days |
| grass_pellets | Grass pellets | $0.80 | 30 days |
| fruit_veg | Fruit and vegetables | $1.15 | 3 days |
| insects | Insects | $3.60 | 2 days |
| bamboo | Bamboo | $0.65 | 5 days |

## Buildings

| ID | Name | Role | Footprint | Capacity | Special use | Cost |
|---|---|---|---:|---:|---|---:|
| small_shelter | Small animal shelter | Animal shelter | 2×2 | 8 | Small species | $1,800 |
| medium_shelter | Herd shelter | Animal shelter | 3×3 | 18 | Medium animals | $4,200 |
| large_shelter | Large animal shelter | Animal shelter | 5×4 | 30 | Large animals | $9,800 |
| heated_barn | Heated tropical barn | Animal shelter | 4×3 | 24 | Heated | $12,500 |
| cooled_den | Cooled polar den | Animal shelter | 4×3 | 20 | Cooled | $13,800 |
| burger_stall | Burger stall | Food stall | 2×2 | 12 | Burger $12, cost $5 | $5,200 |
| drinks_stall | Drinks stall | Drink stall | 2×2 | 12 | Drink $6, cost $1 | $4,100 |
| ice_cream_stand | Ice cream stand | Food stall | 1×2 | 8 | Ice cream $8, cost $2 | $3,300 |
| restroom | Visitor restroom | Restroom | 3×2 | 16 | Visitor facilities | $6,500 |
| gift_shop | Gift shop | Gift shop | 3×3 | 18 | Souvenir $18, cost $7 | $9,200 |
| staff_room | Staff room | Staff room | 3×2 | 10 | Staff rest | $6,800 |
| vet_clinic | Veterinary clinic | Vet clinic | 4×3 | 8 | Animal treatment | $15,000 |
| education_centre | Education centre | Education center | 4×3 | 30 | Education 0.8 | $12,000 |
| research_centre | Research centre | Research center | 5×4 | 12 | Conservation research | $18,500 |

## Props

Trees and bushes add plant cover and shade to the enclosure they stand in, and a little
enrichment (0.03 per tree, 0.02 per bush). A tree occupies only its trunk on the ground, so
animals and visitors walk under its crown. This table is generated: run
`tools/docs/props_table.py` after editing `data/construction/props.json`.

| ID | Name | Role | Key values | Cost |
|---|---|---|---|---:|
| oak | Oak tree | Tree, temperate | Cover 28 m²; shade 40 m²; 12 m tall; trunk 0.6 m | $240 |
| birch | Silver birch | Tree, temperate | Cover 14 m²; shade 16 m²; 10 m tall; trunk 0.35 m | $150 |
| pine | Pine tree | Tree, boreal | Cover 14 m²; shade 14 m²; 14 m tall; trunk 0.45 m | $160 |
| spruce | Spruce tree | Tree, boreal | Cover 16 m²; shade 12 m²; 13 m tall; trunk 0.45 m | $170 |
| acacia | Umbrella acacia | Tree, savanna | Cover 30 m²; shade 45 m²; 9 m tall; trunk 0.5 m | $260 |
| baobab | Baobab | Tree, savanna | Cover 24 m²; shade 40 m²; 13 m tall; trunk 1.6 m | $520 |
| palm | Fan palm | Tree, tropical | Cover 9 m²; shade 10 m²; 11 m tall; trunk 0.35 m | $220 |
| bamboo_clump | Bamboo clump | Tree, tropical | Cover 9 m²; shade 4 m²; 8 m tall; trunk 1 m | $130 |
| willow | Weeping willow | Tree, temperate | Cover 26 m²; shade 36 m²; 10 m tall; trunk 0.6 m | $280 |
| juniper | Juniper bush | Bush, boreal | Cover 4 m²; shade 0.5 m² | $60 |
| savanna_shrub | Savanna shrub | Bush, savanna | Cover 4 m²; shade 0.5 m² | $60 |
| desert_scrub | Desert scrub | Bush, desert | Cover 3 m²; shade 0.3 m² | $50 |
| tropical_fern | Tropical fern | Bush, tropical | Cover 4 m²; shade 0.5 m² | $70 |
| granite_boulder | Granite boulder | Rock | Enrichment 0.08 | $180 |
| sandstone_rock | Sandstone rock | Rock | Enrichment 0.06 | $140 |
| ice_rock | Ice boulder | Rock | Enrichment 0.10 | $260 |
| fallen_log | Fallen log | Enrichment | cover 1 m²; Enrichment 0.18 | $120 |
| meat_platform | Meat platform | Feeder | Meat, 18 kg | $260 |
| hay_rack | Hay rack | Feeder | Hay, 35 kg | $260 |
| pellet_fruit_trough | Pellet and fruit trough | Feeder | Grass pellets/Fruit and vegetables, 24 kg | $260 |
| giraffe_browse_feeder | High browse feeder | Feeder | Leafy browse, 20 kg at 4.5 m | $260 |
| fish_bucket_stand | Fish bucket stand | Feeder | Fish, 14 kg | $260 |
| insect_feeder | Insect feeder | Feeder | Insects, 8 kg | $260 |
| water_trough | Water trough | Water trough | 180 L | $320 |
| ball | Enrichment ball | Enrichment | Enrichment 0.25 | $95 |
| scratching_post | Scratching post | Enrichment | Enrichment 0.32 | $160 |
| climbing_frame | Climbing frame | Enrichment | Enrichment 0.45 | $620 |
| tyre_swing | Tyre swing | Enrichment | Enrichment 0.40 | $240 |
| log_pile | Log pile | Enrichment | Enrichment 0.30 | $180 |
| termite_mound | Termite mound | Enrichment | Enrichment 0.38 | $230 |
| ice_block | Ice block | Enrichment | Enrichment 0.30 | $300 |
| bench | Park bench | Bench | 3 seats | $240 |
| picnic_table | Picnic table | Bench | 6 seats | $390 |
| litter_bin | Litter bin | Bin | 35 items | $110 |
| lamp | Path lamp | Lamp | 3.5 m | $280 |
| info_board | Information board | Info board | Public education | $210 |
| flower_bed | Flower bed | Decoration | Beauty 0.28; walk-through | $190 |
| fountain | Fountain | Decoration | Beauty 0.40 | $1,250 |
| statue | Wildlife statue | Decoration | Beauty 0.25 | $720 |

## Renderer look identifiers

`hut`, `barn`, `kiosk`, `house`, `pavilion`, `cave`, `oak`, `birch`, `pine`, `spruce`, `acacia`, `baobab`, `palm`, `bamboo`, `willow`, `juniper`, `shrub`, `scrub`, `fern`, `boulder`, `rock`, `ice_rock`, `log`, `meat_platform`, `hay_rack`, `trough`, `high_browse`, `fish_bucket`, `insect_feeder`, `ball`, `scratching_post`, `climbing_frame`, `tyre_swing`, `log_pile`, `termite_mound`, `ice_block`, `bench`, `picnic_table`, `bin`, `lamp`, `info_board`, `flower_bed`, `fountain`, `statue`.

## Balancing

The $50,000 starting budget can buy a basic small shelter and a secure enclosure boundary while leaving room for a first modest animal group and visitor amenities. Fence options trade price, visibility, strength and upkeep: inexpensive rails suit low-risk species, while stronger mesh, bars and cable handle progressively more demanding animals. Glass is deliberately costly because it combines strong containment with excellent viewing.

Food prices reflect wholesale staples and perishables; hay and pellets are cheap to store, while meat, fish and insects cost more and spoil quickly. Visitor stalls are priced to turn a small margin on a roughly $20 ticket economy, and their capacities and service times limit how many guests can be served at once. Shelter and care buildings carry meaningful daily upkeep, so opening every service building immediately is not the efficient choice.

Trees and bushes cost more than ground repainting because they add habitat cover, shade and visual variety. Their biome tags help players match planting to native habitats, and enrichment props have modest one-time costs so animal welfare can be improved in stages. Visitor decorations are optional investments whose costs sit between basic seating and larger attractions such as fountains.
