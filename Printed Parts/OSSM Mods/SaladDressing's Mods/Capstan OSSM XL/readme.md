
**Note**: The AIO version of this machine displayed in the pictures, including the mounting of the control board in the front with cables routing to the Motor in the back, is based on @thearmpit idea. The AIO Backpack Cover part in his design is covered by CC BY-NC-SA 4.0 license. The part used in this machine is **not** the same part, it is its own custom part, designed by me for this machine . It merely uses a similar design concept as his part. My AIO Cover part is not covered under CC BY-NC-SA 4.0 license.


# Capstan-OSSM-XL
![IMG20250717163516](https://github.com/user-attachments/assets/c26ea6da-c220-4e42-8925-d1a913f3a561)

Revamp of the OSSM drive mechanism to use a capstan based version, enabling higher load use with a more powerful motor, namely the 60AIM40F.

A Capstan Drive is based on rope and a pulley. For a nice video explaining the mechanism, check this awesome video by Aaed Musa: [https://www.youtube.com/watch?v=MwIBTbumd1Q](https://www.youtube.com/watch?v=MwIBTbumd1Q "https://www.youtube.com/watch?v=MwIBTbumd1Q")

**Why Capstan?** 
The belt driven Version has shown for myself and quite a few others limitations. At higher torques, it would sometimes slip. Bear in mind, it could be that is the result from build or other errors.

**Comparison to the standard OSSM**

Pro:

 - same form factor
 - as quiet or quieter
 - only 13cm of effective rail length loss (e.g. 450mm rail -> 32cm stroke)
 - 2.7 times stronger (32kg compared to 12kg)
 - same speed
 - no slipping
 - no custom pcbs required
 - PitClamp compatible
 - compatible with all custom end effectors

Con:

 - harder to build
 - bigger motor -> more expensive (~110€ shipped incl. tax to germany)
 - more to print
 - in extreme use cases there may be slipping of the rail over time, is fixed by rerunning homing
 - only tried and tested by me so far
 
This machine has served me very well for the past months now, I am personally very happy with it. Provide feedback an suggestions in the discord thread.

Thread: https://discord.com/channels/559409652425687041/1395456804464623817
# AIO vs Normal
The AIO mod version mounts the control board in the front of the motor head. For the normal version there is no mounting at all, and no specific build instructions. I recommend building the AIO version, but if you want to make your own mounting solution, you can print the normal version.
# Pulley Versions
The bushing pulley is better. Make that one if you can find a bushing of the appropriate size and don't mind gluing with epoxy. It is better in every way. If you can't or decide against, the 3d printed pulley does its job, but it will deform over time under the constant force. You will need to replace it when that happens. Also only supports rail length up to 400mm.

# Rail Lengths
With the bushing pulley, it was tested for 450mm. Shorter is always possible. 500mm is probably doable as well, has not been tested. Even higher lengths I do not recommend.
# Rope
Use the rope I list (Liros DC pro 161). If you can't, try to source another dyneema dm20, sk99, sk75 max. diameter 1mm rope.

**DO NOT** use generic uhwmpe from aliexpress. Experiments from kind @neos have shown that the quality is shit. It stretches and breaks with load. It is essential that the rope stretches as little as possible. Lots of research has been done to find the correct rope. Stick to it. Or deviate at your own risk and cost.
# Printing
I have included .3mf file with configured settings. As for the AIO_cover part, you can try printing without supports as included. it's jsut bridges. If it gets too messy or doesn't work feel free to enable supports of course. Some files don't have .3mf files. Those are quite straight forward to print. I always recommend 6 walls 20% infill.
# BOM

| Part                                                                         | Cost (incl. shipping & tax)   | link                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
|------------------------------------------------------------------------------|-------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 60AIM40F                                                                     | ~110€                         | https://www.superbuy.com/en/page/buy/?url=https://item.taobao.com/item.htm?id=668578585661&ali_trackid=2:mm_117358474_33384934_118778578&spm=1101.1101.N.N.e529fe8&__cf_chl_rt_tk=xO29V7EyaAm9af1TaVNaUj1IIxXuPHjLH7hoQn0mA.4-1752658384-1.0.1.1-6hZrr6cDSeufh8ABLvHqqCRzJZHX9yJaKNdapTjhZ1k                                                                                                                                                                                                 |
| OSSM Control Board + Remote (can buy board only if you want wireless remote) | 91 USD (excl. tax & shipping) | www.researchanddesire.com/products/ossm-reference-board                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| 5x MR115 bearings                                                            | ~3€                           | https://de.aliexpress.com/item/1005007175995775.html?spm=a2g0o.productlist.main.2.1a42906C906CMX&algo_pvid=3c5cd0a6-d355-43fd-9cde-0a5c00f7b036&algo_exp_id=3c5cd0a6-d355-43fd-9cde-0a5c00f7b036-1&pdp_ext_f=%7B%22order%22%3A%22100%22%2C%22eval%22%3A%221%22%7D&pdp_npi=4%40dis%21EUR%213.77%212.49%21%21%2130.70%2120.28%21%40211b807017526658700751189e7276%2112000039706894193%21sea%21DE%213286768289%21X&curPageLogUid=LOVHlkqLUfbN&utparam-url=scene%3Asearch%7Cquery_from%3A        |
| Hardware                                                                     | ~30€                          | see hardware/Hardware.txt                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| 1kg PLA                                                                      | ~8€\kg                        | https://de.aliexpress.com/item/1005006639640810.html?spm=a2g0o.order_list.order_list_main.36.4e9b5c5frMJNth&gatewayAdapt=glo2deu                                                                                                                                                                                                                                                                                                                                                             |
| 5m Liros DC pro 161 rope (or similar)                                        | ~15€                          | https://www.kitelineshop.com/ligne-liros-dcpro161-au-metre-c2x38222201                                                                                                                                                                                                                                                                                                                                                                                                                       |
| Bushing 20x23x30 (id x od x l)                                               | ~5€                           | https://www.agrolager.de/product_info.php?products_id=91534519                                                                                                                                                                                                                                                                                                                                                                                                                               |
| MGN12H Linear Rail + Wagon                                                   | ~10-15€ (depending on length) | 450mm: https://de.aliexpress.com/item/1000007480470.html?spm=a2g0o.productlist.main.1.5ff156f48357Wx&algo_pvid=51755ed2-c0a1-4bff-bf30-97a3b97270f5&algo_exp_id=51755ed2-c0a1-4bff-bf30-97a3b97270f5-0&pdp_ext_f=%7B%22order%22%3A%221088%22%2C%22eval%22%3A%221%22%7D&pdp_npi=4%40dis%21EUR%2114.99%2110.79%21%21%2117.00%2112.24%21%40211b80d117526659739281539ed9d5%2112000031932165347%21sea%21DE%213286768289%21X&curPageLogUid=8jDEp4Cr4fcW&utparam-url=scene%3Asearch%7Cquery_from%3A |
| Epoxy glue                                                                   | ~3€                           | https://de.aliexpress.com/item/1005007115129874.html?spm=a2g0o.productlist.main.1.625c4571tg9Mio&algo_pvid=102cafb2-86c6-400a-bd2e-ed3b473bc5f1&algo_exp_id=102cafb2-86c6-400a-bd2e-ed3b473bc5f1-0&pdp_ext_f=%7B%22order%22%3A%222237%22%2C%22eval%22%3A%221%22%7D&pdp_npi=4%40dis%21EUR%218.50%212.89%21%21%2169.22%2123.54%21%40210391a017526660236821518e0e4b%2112000039451196262%21sea%21DE%213286768289%21X&curPageLogUid=jgnbjFDGCHXZ&utparam-url=scene%3Asearch%7Cquery_from%3A       |
| 4 pin JST header cable                                                       | ~2€                           | https://de.aliexpress.com/item/1005007389108799.html?spm=a2g0o.productlist.main.1.52b15f2cbLDcTR&algo_pvid=7345200b-9cbe-46bc-b73c-8940f0cbcc8e&algo_exp_id=7345200b-9cbe-46bc-b73c-8940f0cbcc8e-0&pdp_ext_f=%7B%22order%22%3A%223138%22%2C%22eval%22%3A%221%22%7D&pdp_npi=4%40dis%21EUR%211.14%211.09%21%21%211.29%211.23%21%40211b80f717526660722672942e4eec%2112000040551940967%21sea%21DE%213286768289%21X&curPageLogUid=yI8x8KQg3Oo5&utparam-url=scene%3Asearch%7Cquery_from%3A         |
| 2 pin awg 18 or thicker 30cm                                                 | ~2€                           | https://de.aliexpress.com/item/1005006614755156.html?algo_pvid=930912fc-6f0e-4707-9a81-045ecb65c04d&algo_exp_id=930912fc-6f0e-4707-9a81-045ecb65c04d-1&pdp_ext_f=%7B%22order%22%3A%222628%22%2C%22eval%22%3A%221%22%7D&pdp_npi=4%40dis%21EUR%214.55%214.29%21%21%2137.05%2134.93%21%40211b431017526666257724421e6df9%2112000037830294865%21sea%21DE%213286768289%21X&curPageLogUid=kgtrU9AoDa9b&utparam-url=scene%3Asearch%7Cquery_from%3A                                                   |
| Kinesio Tape                                                                 | ~3€                           | buy locally (e.g. drug store)                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| 36V 5+A power supply (GST220A36-R7B + r7bf-p1j adapter)                      | ~68€                          | https://www.voelkner.de/products/2995507/MW-Mean-Well-GST220A36-R7B-Tischnetzteil-Festspannung-36-V-DC-6.1A-219.6W.html + https://www.voelkner.de/products/6716585/MW-Mean-Well-DC-PLUG-R7BF-P1J-Adapter.html?offer=2a0e3584bd7903f1cb12fce88b080532                                                                                                                                                                                                                                         |
| **Total**                                                                    | **~345€**                         |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |

Finally, special thanks to everyone that helped me and gave suggestions when I hit a wall!
