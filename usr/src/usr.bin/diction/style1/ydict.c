/*-
 * This module is believed to contain source code proprietary to AT&T.
 * Use and redistribution is subject to the Berkeley Software License
 * Agreement and your Software Agreement with AT&T (Western Electric).
 *
 *	@(#)ydict.c	8.1 (Berkeley) 6/6/93
 */

struct dict ary_d[] = {
"auxili",'Y',
"benefici",'Y',
"bin",'Y',
"capill",'Y',
"caten",'Y',
"constabul",'Y',
"contempor",'Y',
"contr",'Y',
"coroll",'Y',
"coron",'Y',
"diet",'Y',
"dignit",'Y',
"document",'Y',
"formul",'Y',
"honor",'Y',
"incendi",'Y',
"intermedi",'Y',
"itiner",'Y',
"judici",'Y',
"lapid",'Y',
"legion",'Y',
"lumin",'Y',
"maxill",'Y',
"mercen",'Y',
"milit",'Y',
"millen",'Y',
"mission",'Y',
"mortu",'Y',
"obitu",'Y',
"offici",'Y',
"ordin",'Y',
"penitenti",'Y',
"pension",'Y',
"plenipotenti",'Y',
"prelimin",'Y',
"prim",'Y',
"propriet",'Y',
"reaction",'Y',
"revolution",'Y',
"rot",'Y',
"second",'Y',
"semidocument",'Y',
"solit",'Y',
"statu",'Y',
"subsidi",'Y',
"summ",'Y',
"tribut",'Y',
"usufructu",'Y',
"veterin",'Y',
"vision",'Y',
"actu",'N',
"advers",'N',
"ang",'N',
"annivers",'N',
"antiphon",'N',
"antiqu",'N',
"api",'N',
"apothec",'N',
"avi",'N',
"begg",'N',
"bound",'N',
"burgl",'N',
"burs",'N',
"calv",'N',
"can",'N',
"capitul",'N',
"comment",'N',
"commiss",'N',
"deposit",'N',
"di",'N',
"diction",'N',
"dispens",'N',
"distribut",'N',
"dromed",'N',
"emiss",'N',
"estu",'N',
"function",'N',
"gloss",'N',
"gran",'N',
"infirm",'N',
"libr",'N',
"mandat",'N',
"ossu",'N',
"osti",'N',
"ov",'N',
"parcen",'N',
"pisc",'N',
"plagi",'N',
"quand",'N',
"question",'N',
"ros",'N',
"rosem",'N',
"sal",'N',
"sanctu",'N',
"secret",'N',
"semin",'N',
"syllab",'N',
"undersecret",'N',
"vag",'N',
"vocabul",'N',
"centen",'Y',
"counterrevolution",'Y',
"insurrection",'Y',
"v",'V',
"overwe",'H',
"we",'H',
0, 0
};

struct dict cy_d[] = {
"fan",'U',
"mer",'Y',
"regen",'Y',
"boun",'J',
"chan",'J',
"flee",'J',
"i",'J',
"jui",'J',
"la",'J',
"ra",'J',
"sau",'J',
"spi",'J',
0,0
};

struct dict ery_d[] = {
"fi",'J',
"qu",'Z',
"be",'J',
"blist",'J',
"blubb",'J',
"blust",'J',
"bri",'J',
"che",'J',
"cind",'J',
"clatt",'J',
"clust",'J',
"copp",'J',
"feath",'J',
"flick",'J',
"flow",'J',
"flutt",'J',
"ging",'J',
"glitt",'J',
"heath",'J',
"jasp",'J',
"jitt",'J',
"lath",'J',
"leath",'J',
"le",'J',
"litt",'J',
"low",'J',
"orn",'J',
"pap",'J',
"pepp",'J',
"plast",'J',
"powd",'J',
"puck",'J',
"quav",'J',
"rubb",'J',
"shimm",'J',
"shiv",'J',
"show",'J',
"shudd",'J',
"silv",'J',
"slipp",'J',
"slith",'J',
"slobb",'J',
"slumb",'J',
"smoth",'J',
"spid",'J',
"splint",'J',
"summ",'J',
"tot",'J',
"twitt",'J',
"wat",'J',
"whisk",'J',
"whisp",'J',
"bow",'Y',
"butt",'Y',
"liv",'Y',
"trump",'Y',
0,0
};

struct dict fy_d[] = {
"jif",'N',
"taf",'N',
"bee",'J',
"com",'J',
"daf",'J',
"dandruf",'J',
"fluf",'J',
"goo",'J',
"gul",'J',
"huf",'J',
"if",'J',
"lea",'J',
"puf",'J',
"scruf",'J',
"scur",'J',
"snif",'J',
"snuf",'J',
"spif",'J',
"stuf",'J',
"tur",'J',
0,0
};

struct dict gy_d[]  = {
"aller",'N',
"anthropopha",'N',
"bacteriopha",'N',
"bibliope",'N',
"chemur",'N',
"cler",'N',
"cosmolo",'N',
"cytopha",'N',
"dramatur",'N',
"effi",'N',
"electrometallur",'N',
"ele",'N',
"ener",'N',
"lethar",'N',
"litur",'N',
"metallur",'N',
"or",'N',
"por",'N',
"prodi",'N',
"strate",'N',
"syner",'N',
"syzy",'N',
"thaumatur",'N',
"zymur",'N',
"bug",'Y',
"dog",'Y',
0,0
};

struct dict ity_d[]  = {
"util",'Y',
"fru",'J',
"rabb",'J',
"wh",'J',
"upp",'J',
"p",'Z',
0,0
};

struct dict ly_d[] = {
"beast",'J',
"blackguard",'G',
"bodi",'G',
"clean",'G',
"court",'G',
"coward",'G',
"dead",'G',
"ear",'G',
"easter",'G',
"father",'J',
"friend",'G',
"ghast",'G',
"ghost",'J',
"ginger",'G',
"god",'J',
"heaven",'J',
"hour",'G',
"kind",'G',
"king",'G',
"knight",'G',
"laggard",'G',
"leisure",'G',
"like",'G',
"live",'G',
"loath",'G',
"lord",'G',
"lover",'G',
"low",'G',
"man",'G',
"manner",'G',
"master",'G',
"minute",'G',
"night",'G',
"northeaster",'G',
"northwester",'G',
"on",'G',
"poor",'G',
"prince",'G',
"rascal",'G',
"seem",'G',
"sight",'G',
"sister",'G',
"slattern",'G',
"sloven",'G',
"soldier",'G',
"southeaster",'G',
"souther",'G',
"southwester",'G',
"state",'G',
"sur",'G',
"time",'G',
"unclean",'G',
"unkind",'G',
"unmanner",'G',
"unseem",'G',
"untime",'G',
"weak",'G',
"wester",'U',
"woman",'G',
"year",'G',
"bubb",'Y',
"fami",'Y',
"li",'Y',
"melancho",'Y',
"wool",'Y',
"dai",'G',
"al",'Z',
"bel",'N',
"dol",'Z',
"gul",'Z',
"jel",'Z',
"oversupp",'Z',
"p",'Z',
"ral",'Z',
"rep",'Z',
"sal",'Z',
"sul",'Z',
"supp",'Z',
"tal",'Z',
"bastard",'J',
"beggar",'J',
"bramb",'J',
"brist",'J',
"brother",'J',
"bur",'J',
"butcher",'J',
"child",'J',
"chil",'G',
"church",'J',
"citizen",'J',
"colonial",'J',
"come",'J',
"cost",'J',
"crack",'J',
"craw",'J',
"creature",'J',
"crink",'J',
"crumb",'J',
"cudd",'J',
"cur",'J',
"dastard",'J',
"dimp",'J',
"drizz",'J',
"earth",'J',
"ee",'J',
"elder",'J',
"flannel",'J',
"flesh",'J',
"friar",'J',
"fril",'J',
"frizz",'J',
"gain",'J',
"gang",'J',
"gentleman",'J',
"gigg",'J',
"gnar",'J',
"gogg",'J',
"good",'J',
"gravel",'J',
"gris",'J',
"grist",'J',
"grizz",'J',
"grumb",'J',
"hack",'J',
"hazel",'J',
"hil",'J',
"ho",'J',
"home",'J',
"iridescent",'J',
"jewel",'J',
"jigg",'J',
"jing",'J',
"jow",'J',
"jung",'J',
"kinesthetical",'J',
"knur",'J',
"laird",'J',
"lavish",'J',
"lone",'J',
"maiden",'J',
"marb",'J',
"mar",'J',
"matron",'J',
"mea",'J',
"meas",'J',
"miser",'J',
"mizz",'J',
"mother",'J',
"neighbor",'J',
"nubb",'J',
"oi",'J',
"otherworld",'J',
"painter",'J',
"pal",'J',
"pa",'J',
"pear",'J',
"pebb",'J',
"pimp",'J',
"port",'J',
"prick",'J',
"priest",'J',
"ratt",'J',
"roi",'J',
"rubb",'J',
"ruffian",'J',
"ruff",'J',
"rumb",'J',
"rump",'J',
"saint",'J',
"sca",'J',
"scholar",'J',
"scoundrel",'J',
"scrabb",'J',
"scragg",'J',
"scraw",'J',
"seaman",'J',
"shape",'J',
"shel",'J',
"shing",'J',
"sil",'J',
"sluggard",'J',
"s",'J',
"smel",'J',
"snar",'J',
"southern",'J',
"spind",'J',
"sportsman",'J',
"spright",'J',
"squal",'J',
"squirrel",'J',
"statesman",'J',
"stee",'J',
"stragg",'J',
"stubb",'J',
"swir",'J',
"tang",'J',
"thegn",'J',
"thist",'J',
"tink",'J',
"tinsel",'J',
"treac",'J',
"tremb",'J',
"twink",'J',
"twir",'J',
"ug",'J',
"unearth",'J',
"unfriend",'J',
"ungain",'J',
"ungod",'J',
"unho",'J',
"unlike",'J',
"unlove",'J',
"unman",'J',
"unru",'J',
"unsight",'J',
"unworld",'J',
"vea",'J',
"wagg",'J',
"wal",'J',
"weather",'J',
"wife",'J',
"wigg",'J',
"wi",'J',
"winter",'J',
"wizard",'J',
"wobb",'J',
"world",'J',
"wrigg",'J',
"wrink",'J',
"anoma",'N',
"assemb",'N',
"bibliophi",'N',
"bil",'N',
"brachycepha",'N',
"butterf",'N',
"contume",'N',
"dil",'N',
"doi",'N',
"duopo",'N',
"fil",'N',
"firef",'N',
"fol",'N',
"hillbil",'N',
"hol",'N',
"homi",'N',
"hur",'N',
"lol",'N',
"monopo",'N',
"norther",'U',
"philate",'N',
"potbel",'N',
"tel",'N',
"app",'V',
"comp",'V',
"dal",'V',
"dillydal",'V',
"imp",'V',
"misapp",'V',
"overf",'V',
"re",'V',
"bul",'U',
"f",'U',
"multip",'U',
"fortnight",'G',
"jol",'G',
"love",'G',
"month",'G',
"bimonth",'G',
"order",'Y',
"disorder",'J',
"quarter",'G',
"sick",'G',
"semiweek",'G',
"week",'G',
"biweek",'G',
0,0
};

struct dict ory_d[] = {
"access",'Y',
"advis",'Y',
"ambulat",'Y',
"cremat",'Y',
"depilat",'Y',
"direct",'Y',
"interrogat",'Y',
"lavat",'Y',
"mandat",'Y',
"reformat",'Y',
"refract",'Y',
"reposit",'Y',
"salutat",'Y',
"signat",'Y',
"valedict",'Y',
"alleg",'N',
"arm",'N',
"categ",'N',
"chic",'N',
"conservat",'N',
"cosignat",'N',
"deposit",'N',
"dispensat",'N',
"dormit",'N',
"d",'N',
"emunct",'N',
"fact",'N',
"gl",'Z',
"hick",'N',
"hist",'N',
"invent",'Z',
"iv",'N',
"judicat",'N',
"laborat",'N',
"l",'N',
"manufact",'N',
"mem",'N',
"observat",'N',
"offert",'N',
"orat",'N',
"pill",'Z',
"prehist",'N',
"pri",'N',
"protect",'N',
"purgat",'N',
"rect",'N',
"repert",'N',
"st",'Z',
"succ",'N',
"supposit",'N',
"territ",'N',
"the",'N',
"traject",'N',
"vaingl",'N',
"vict",'N',
"vomit",'N',
"consummat",'D',
"preparat",'G',
0,0
};

struct dict ry_d[] = {
"d",'H',
"fai",'Y',
"luxu",'Y',
"tawd",'J',
"ai",'J',
"ang",'J',
"blur",'J',
"bur",'J',
"count",'Y',
"fir",'J',
"fleu",'J',
"flou",'J',
"fur",'J',
"glai",'J',
"hai",'J',
"hung",'J',
"mer",'J',
"mi",'J',
"palt",'J',
"scar",'J',
"sor",'J',
"spi",'J',
"sp",'J',
"star",'J',
"sult",'J',
"sund",'J',
"wint",'J',
"wi",'J',
"ber",'Z',
"car",'Z',
"c",'Z',
"desc",'V',
"fer",'Z',
"flur",'Z',
"f",'Z',
"hur",'Z',
"par",'Z',
"p",'Z',
"quar",'Z',
"scur",'Z',
"tar",'H',
"t",'Z',
"wor",'Z',
"bu",'V',
"cur",'Z',
"dec",'V',
"har",'V',
"intermar",'V',
"mar",'V',
"miscar",'V',
"ser",'V',
"whir",'V',
"w",'H',
"aw",'D',
0,0
};

struct dict ty_d[] = {
"jet",'U',
"pret",'U',
"cat",'Y',
"dain",'Y',
"hear",'Y',
"nif",'Y',
"par",'Y',
"pas",'Y',
"penal",'Y',
"plen",'Y',
"pot",'Y',
"socie",'Y',
"trus",'Y',
"admiral",'N',
"anxie",'N',
"beau",'N',
"boo",'N',
"boun",'N',
"casual",'N',
"certain",'N',
"champer",'N',
"commonal",'N',
"contrarie",'N',
"coun",'N',
"cruel",'N',
"depu",'N',
"difficul",'N',
"dishones",'N',
"disloyal",'N',
"dit",'N',
"dubie",'N',
"du",'N',
"dynas",'N',
"entire",'N',
"entrea",'N',
"facul",'N',
"frail",'N',
"gaie",'N',
"hones",'N',
"immodes",'N',
"impie",'N',
"improprie",'N',
"indigestibil",'N',
"inebrie",'N',
"kit",'N',
"liber",'N',
"loyal",'N',
"majes",'N',
"mayoral",'N',
"modes",'N',
"moie",'N',
"naive",'N',
"nice",'N',
"nimie",'N',
"notorie",'N',
"novel",'N',
"pat",'N',
"pederas",'N',
"peripe",'N',
"personal",'N',
"pie",'N',
"pigs",'N',
"pover",'N',
"proper",'N',
"proprie",'N',
"puber",'N',
"pun",'N',
"real",'N',
"royal",'N',
"sacris",'N',
"satie",'N',
"sergean",'N',
"several",'N',
"shan",'N',
"shel",'N',
"shrieval",'N',
"smar",'N',
"sobrie",'N',
"sof",'N',
"sovereign",'N',
"special",'N',
"spiritual",'N',
"subcontrarie",'N',
"subtil",'N',
"subtle",'N',
"sure",'N',
"trea",'N',
"tut",'N',
"uncertain",'N',
"unsafe",'N',
"varie",'N',
"viceroyal",'N',
"viscoun",'N',
"warran",'N',
"zlo",'N',
"amnes",'Z',
"dir",'H',
"emp",'H',
"guaran",'Z',
"jut",'Z',
"put",'Z',
"safe",'Z',
"s",'Z',
"traves",'Z',
"migh",'G',
"twis",'D',
"eigh",'Y',
"fif",'Y',
"for",'Y',
"nine",'Y',
"seven",'Y',
"six",'Y',
"thir",'Y',
"twen",'Y',
0,0
};
