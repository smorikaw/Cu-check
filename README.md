**Copper SFP PHY register check tool**
for Rasberry Pi
2023/12/25

1. method Clause 22 via I2C
1. metohd Clause 45 via I2C

光のトランシーバーと異なる、UTP/Copper対応のトランシーバーを使うにはPHY固有のレジスタ操作が必要になることがあります。
* １）レジスタは各社共通のものと、独自のものがあります。
* ２）レジスタへのアクセス方法はClause 22とClause 45の二つの方法があります
IEEE802.3にて定義されているMDIO Clause 22,45ですがMDIOのビット列の表記とI2Cでの仕様例の対応が判り難い。
* ３）どのPHYを使用して居るかはPHY IDを判別して行いますが、PHY IDの値がPHYのデータシートの記載と異なるものがあります

Clause 22はレジスタアドレスは8bit、値は16bit。0-15が共通。16-31がベンダー固有
Clause 45はレジスタアドレス16bit、値は16bit共通レジスタ 0-15
- 0x0000 :
- 0x0001 : status
- 0x0002 : PHY ID上位16bit
- 0x0003 : PHY ID下位16bit
PHY ID
1000BASE-Tで最も一般的なMARVELL Alaska 88E1111の値は0x1410cc0、最下位8bitはrevisionを示すため複数の値があります。

MII MDIOとI2Cの違い
MDIOがbit列単位の定義で有りI2Cはbyte単位ではあるが、ビット列に変換する事が可能。

Clause　30

+ /* RollBall SFPs do not access internal PHY via I2C address 0x56, but
+ * instead via address 0x51, when SFP page is set to 0x03 and password to
+ * 0xffffffff:
+ *
+ * address  size  contents  description
+ * -------  ----  --------  -----------
+ * 0x80     1     CMD       0x01/0x02/0x04 for write/read/done
+ * 0x81     1     DEV       Clause 45 device
+ * 0x82     2     REG       Clause 45 register
+ * 0x84     2     VAL       Register value
+ */
