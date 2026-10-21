#!/usr/bin/env python3
"""Build the offline special-symbol catalogue from Unicode character data.

Symbols are plain text: no emoji variation selector is appended, so the target
application keeps its own font. Only Python's ``unicodedata`` is required; the
optional ICU ``uconv`` adds a pinyin index. No network access is needed.
"""
import json
import subprocess
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATEGORIES = ["常用符号", "标点符号", "货币符号", "数学符号", "箭头符号", "几何图形", "字母与数字", "其他符号"]

# Hand-picked high-frequency symbols, in display order. They also seed the
# common tab; the ranges below fill the remaining tabs.
COMMON = [
    ("©", "版权符号", "copyright banquan"), ("®", "注册商标", "registered trademark zhuce"),
    ("™", "商标", "trademark tm shangbiao"), ("℠", "服务商标", "service mark fuwushangbiao"),
    ("℗", "录音版权", "sound recording copyright luyin"),
    ("°", "度", "degree du"), ("℃", "摄氏度", "celsius degree sheshidu"), ("℉", "华氏度", "fahrenheit huashidu"),
    ("±", "正负号", "plus minus zhengfu"), ("×", "乘号", "multiplication multiply chenghao"),
    ("÷", "除号", "division divide chuhao"), ("≠", "不等号", "not equal budenghao"),
    ("≈", "约等于", "approximately yuedengyu"), ("≡", "恒等", "identical hengdeng"),
    ("≤", "小于等于", "less than or equal xiaoyudengyu"), ("≥", "大于等于", "greater than or equal dayudengyu"),
    ("∞", "无穷", "infinity wuqiong"), ("√", "根号", "square root genhao"),
    ("∑", "求和", "summation qiuhe"), ("∏", "连乘", "product liancheng"), ("∫", "积分", "integral jifen"),
    ("∂", "偏导", "partial derivative piandao"), ("∆", "增量", "increment delta zengliang"),
    ("∇", "梯度", "nabla gradient tidu"), ("∈", "属于", "element of shuyu"),
    ("∉", "不属于", "not element of bushuyu"), ("∅", "空集", "empty set kongji"),
    ("∀", "任意", "for all renyi"), ("∃", "存在", "exists cunzai"), ("∴", "所以", "therefore suoyi"),
    ("∵", "因为", "because yinwei"), ("∠", "角", "angle jiao"), ("⊥", "垂直", "perpendicular chuizhi"),
    ("≅", "全等", "congruent quandeng"), ("∝", "正比", "proportional zhengbi"),
    ("⊙", "同心圆", "circled dot tongxinyuan"), ("⊕", "圈加", "circled plus quanji"),
    ("α", "阿尔法", "alpha aerfa"), ("β", "贝塔", "beta beita"), ("γ", "伽马", "gamma gama"),
    ("δ", "德尔塔", "delta deerta"), ("θ", "西塔", "theta xita"), ("λ", "拉姆达", "lambda lamuda"),
    ("μ", "缪", "mu miu"), ("π", "圆周率", "pi yuanzhoulv"), ("σ", "西格玛", "sigma xigema"),
    ("φ", "斐", "phi fei"), ("ω", "欧米伽", "omega oumijia"), ("Ω", "欧米伽", "omega oumijia"),
    ("Γ", "伽马", "gamma gama"), ("Δ", "德尔塔", "delta deerta"), ("Σ", "西格玛", "sigma xigema"),
    ("Φ", "斐", "phi fei"), ("Ψ", "普西", "psi puxi"),
    ("—", "破折号", "em dash pozhehao"), ("–", "连接号", "en dash lianjiehao"),
    ("…", "省略号", "ellipsis shengluehao"), ("·", "间隔号", "middle dot jiangehao"),
    ("•", "项目符号", "bullet xiangmufuhao"),
    ("“", "左双引号", "left double quotation zuoshuangyinhao"),
    ("”", "右双引号", "right double quotation youshuangyinhao"),
    ("‘", "左单引号", "left single quotation zuodanyinhao"),
    ("’", "右单引号", "right single quotation youdanyinhao"),
    ("«", "左角引号", "left guillemet zuojiaoyinhao"), ("»", "右角引号", "right guillemet youjiaoyinhao"),
    ("§", "章节号", "section sign zhangjiehao"), ("¶", "段落号", "pilcrow duanluohao"),
    ("†", "剑号", "dagger jianhao"), ("‡", "双剑号", "double dagger shuangjianhao"),
    ("‰", "千分号", "per mille qianfenhao"), ("′", "分", "prime fen"), ("″", "秒", "double prime miao"),
    ("※", "参考标记", "reference mark cankaobiaoji"),
    ("¡", "倒感叹号", "inverted exclamation daogantanhao"), ("¿", "倒问号", "inverted question daowenhao"),
    ("№", "编号", "numero number bianhao"), ("℡", "电话", "telephone dianhua"),
    ("€", "欧元", "euro ouyuan"), ("£", "英镑", "pound yingbang"), ("¥", "人民币日元", "yen yuan renminbi riyuan"),
    ("₹", "印度卢比", "rupee yindulubi"), ("₽", "俄罗斯卢布", "ruble eluosilubu"),
    ("₩", "韩元", "won hanyuan"), ("₪", "以色列谢克尔", "shekel yiseliexiekeer"),
    ("₿", "比特币", "bitcoin bitebi"), ("¢", "美分", "cent meifen"),
    ("←", "左箭头", "left arrow zuojiantou"), ("→", "右箭头", "right arrow youjiantou"),
    ("↑", "上箭头", "up arrow shangjiantou"), ("↓", "下箭头", "down arrow xiajiantou"),
    ("↔", "左右箭头", "left right arrow zuoyoujiantou"), ("↕", "上下箭头", "up down arrow shangxiajiantou"),
    ("↖", "左上箭头", "up left arrow zuoshangjiantou"), ("↗", "右上箭头", "up right arrow youshangjiantou"),
    ("↘", "右下箭头", "down right arrow youxiajiantou"), ("↙", "左下箭头", "down left arrow zuoxiajiantou"),
    ("↩", "左转弯箭头", "leftwards arrow with hook zuozhuanwan"), ("↪", "右转弯箭头", "rightwards arrow with hook youzhuanwan"),
    ("⇐", "双线左箭头", "double left arrow shuangxianzuo"), ("⇒", "双线右箭头", "double right arrow shuangxianyou"),
    ("⇔", "双线双向箭头", "double left right arrow shuangxian"), ("➔", "粗右箭头", "heavy right arrow cuyou"),
    ("➜", "圆头右箭头", "rounded right arrow yuantou"), ("➡", "黑色右箭头", "black right arrow heiseyou"),
    ("⤴", "上弯箭头", "upwards arrow shangwan"), ("⤵", "下弯箭头", "downwards arrow xiawan"),
    ("■", "实心方块", "black square shixinfangkuai"), ("□", "空心方块", "white square kongxinfangkuai"),
    ("▲", "实心上三角", "up pointing triangle shixinshang"), ("△", "空心上三角", "up pointing triangle kongxinshang"),
    ("▼", "实心下三角", "down pointing triangle shixinxia"), ("▽", "空心下三角", "down pointing triangle kongxinxia"),
    ("▶", "实心右三角", "right pointing triangle shixinyou"), ("▷", "空心右三角", "right pointing triangle kongxinyou"),
    ("◀", "实心左三角", "left pointing triangle shixinzuo"), ("◁", "空心左三角", "left pointing triangle kongxinzuo"),
    ("◆", "实心菱形", "black diamond shixinlingxing"), ("◇", "空心菱形", "white diamond kongxinlingxing"),
    ("○", "空心圆", "white circle kongxinyuan"), ("●", "实心圆", "black circle shixinyuan"),
    ("◎", "同心圆", "bullseye tongxinyuan"), ("★", "实心星", "black star shixinxing"), ("☆", "空心星", "white star kongxinxing"),
    ("✓", "对勾", "check mark duigou"), ("✔", "粗对勾", "heavy check mark cuduigou"),
    ("✗", "叉号", "ballot x chahao"), ("✘", "粗叉号", "heavy ballot x cuchahao"),
    ("☑", "勾选方框", "checked box gouxuan"), ("☒", "叉选方框", "crossed box chaxuan"),
    ("☐", "空方框", "empty box kongfangkuang"),
    ("♠", "黑桃", "spade heitao"), ("♣", "梅花", "club meihua"), ("♥", "红桃", "heart hongtao"),
    ("♦", "方块", "diamond fangkuai"), ("♡", "空心红心", "white heart kongxinhongxin"),
    ("♩", "四分音符", "quarter note sifenyinfu"), ("♪", "八分音符", "eighth note bafenyinfu"),
    ("♫", "双八分音符", "beamed eighth notes shuangbafen"), ("♬", "双十六分音符", "beamed sixteenth notes shuangshiliu"),
    ("♭", "降号", "flat jianghao"), ("♮", "还原号", "natural huanyuanhao"), ("♯", "升号", "sharp shenghao"),
    ("☺", "笑脸", "smiling face xiaolian"), ("☻", "实心笑脸", "black smiling face shixinxiaolian"),
    ("☹", "哭脸", "frowning face kualian"),
    ("☀", "太阳", "sun taiyang"), ("☁", "云", "cloud yun"), ("☂", "雨伞", "umbrella yusan"),
    ("☃", "雪人", "snowman xueren"), ("☕", "热饮", "hot beverage reyin"), ("⚡", "闪电", "high voltage lightning shandian"),
    ("⚠", "警告", "warning jinggao"), ("☢", "辐射", "radioactive fushe"), ("☣", "生物危害", "biohazard shengwuweihai"),
    ("♻", "回收", "recycling huishou"), ("⚙", "齿轮", "gear chilun"), ("♨", "温泉", "hot springs wenquan"),
    ("☎", "电话", "telephone dianhua"), ("✂", "剪刀", "scissors jiandao"), ("✈", "飞机", "airplane feiji"),
    ("☘", "三叶草", "shamrock sanyecao"), ("❄", "雪花", "snowflake xuehua"), ("✿", "花", "flower hua"),
    ("❀", "花", "flower hua"),
    ("⌘", "Command 键", "command key"), ("⌥", "Option 键", "option key"), ("⇧", "Shift 键", "shift key"),
    ("⌃", "Control 键", "control key"), ("⏎", "回车", "return enter huiche"),
    ("⌫", "退格", "backspace tuige"), ("⌦", "删除", "delete shanchu"),
    ("⎋", "Esc 键", "escape key"), ("⏏", "弹出", "eject tanchu"), ("⌨", "键盘", "keyboard jianpan"),
    ("♀", "女性", "female nvxing"), ("♂", "男性", "male nanxing"), ("☮", "和平", "peace heping"),
    ("☯", "阴阳", "yin yang yinyang"), ("⚓", "锚", "anchor mao"), ("⚖", "天平", "scales tianping"),
    ("⚔", "交叉剑", "crossed swords jiaochajian"), ("⚛", "原子", "atom yuanzi"),
    ("✎", "铅笔", "pencil qianbi"), ("✏", "铅笔", "pencil qianbi"), ("✒", "钢笔", "pen gangbi"),
    ("✉", "信封", "envelope xinfeng"), ("⚐", "白旗", "white flag baiqi"), ("⚑", "旗帜", "black flag qizhi"),
]

# Chinese names for useful symbols outside the common tab.
CURATED = {
    "¦": ("断线竖线", "broken bar duanxian"), "¨": ("分音符", "diaeresis fenyinfu"),
    "ª": ("阴性序数", "feminine ordinal yinxing"), "¬": ("非", "not sign fei"),
    "¯": ("上横线", "macron shanghengxian"), "´": ("锐音符", "acute accent ruiyinfu"),
    "µ": ("微符号", "micro sign weifuhao"), "¸": ("软音符", "cedilla ruan yinfu"),
    "º": ("阳性序数", "masculine ordinal yangxing"), "¼": ("四分之一", "one quarter sifenzhiyi"),
    "½": ("二分之一", "one half erfenzhiyi"), "¾": ("四分之三", "three quarters sifenzhisan"),
    "¹": ("上标 1", "superscript one shangbiao"), "²": ("上标 2", "superscript two shangbiao"),
    "³": ("上标 3", "superscript three shangbiao"),
    "¤": ("货币符号", "currency sign huobifuhao"),
    "‾": ("上划线", "overline shanghuaxian"), "‿": ("下连结", "undertie xialianjie"),
    "⁀": ("上连结", "character tie shanglianjie"),
    "☭": ("镰刀锤子", "hammer and sickle liandao"),
    "☪": ("星月", "star and crescent xingyue"),
    "♈": ("白羊座", "aries baiyangzuo"), "♉": ("金牛座", "taurus jinniuzuo"),
    "♊": ("双子座", "gemini shuangzizuo"), "♋": ("巨蟹座", "cancer juxiezuo"),
    "♌": ("狮子座", "leo shizizuo"), "♍": ("处女座", "virgo chunvzuo"),
    "♎": ("天秤座", "libra tianchengzuo"), "♏": ("天蝎座", "scorpio tianxiezuo"),
    "♐": ("射手座", "sagittarius sheshouzuo"), "♑": ("摩羯座", "capricorn mojiezuo"),
    "♒": ("水瓶座", "aquarius shuipingzuo"), "♓": ("双鱼座", "pisces shuangyuzuo"),
    "♔": ("白王", "white chess king baiwang"), "♕": ("白后", "white chess queen baihou"),
    "♖": ("白车", "white chess rook baiju"), "♗": ("白象", "white chess bishop baixiang"),
    "♘": ("白马", "white chess knight baima"), "♙": ("白兵", "white chess pawn baibing"),
    "♚": ("黑王", "black chess king heiwang"), "♛": ("黑后", "black chess queen heihou"),
    "♜": ("黑车", "black chess rook heiju"), "♝": ("黑象", "black chess bishop heixiang"),
    "♞": ("黑马", "black chess knight heima"), "♟": ("黑兵", "black chess pawn heibing"),
    "⚀": ("骰子 1", "die face 1 shaizi"), "⚁": ("骰子 2", "die face 2 shaizi"),
    "⚂": ("骰子 3", "die face 3 shaizi"), "⚃": ("骰子 4", "die face 4 shaizi"),
    "⚄": ("骰子 5", "die face 5 shaizi"), "⚅": ("骰子 6", "die face 6 shaizi"),
    "⌚": ("手表", "watch shoubiao"), "⌛": ("沙漏", "hourglass shalou"),
    "⏳": ("沙漏", "hourglass shalou"), "⌨": ("键盘", "keyboard jianpan"),
    "⏏": ("弹出", "eject tanchu"), "⏭": ("下一曲", "next track xiayiqu"),
    "⏮": ("上一曲", "previous track shangyiqu"), "⏯": ("播放暂停", "play pause bofangzanting"),
    "⏸": ("暂停", "pause zanting"), "⏹": ("停止", "stop tingzhi"), "⏺": ("录制", "record luzhi"),
    "☰": ("三横线", "trigram heaven sanhengxian"), "☷": ("三断线", "trigram earth sanduanxian"),
    "☺": ("笑脸", "smiling face xiaolian"), "☻": ("实心笑脸", "black smiling face shixinxiaolian"),
    "☼": ("太阳", "white sun taiyang"), "☽": ("上弦月", "first quarter moon shangxianyue"),
    "☾": ("下弦月", "last quarter moon xiaxianyue"), "♁": ("地球", "earth diqiu"),
    "♃": ("木星", "jupiter muxing"), "♄": ("土星", "saturn tuxing"), "♅": ("天王星", "uranus tianwangxing"),
    "♆": ("海王星", "neptune haiwangxing"), "♇": ("冥王星", "pluto mingwangxing"),
    "⚑": ("旗帜", "black flag qizhi"), "⚐": ("白旗", "white flag baiqi"),
    "☔": ("雨滴", "umbrella with rain yudi"), "☕": ("热饮", "hot beverage reyin"),
    "⚽": ("足球", "soccer ball zuqiu"), "⚾": ("棒球", "baseball bangqiu"),
    "⛄": ("雪人", "snowman without snow xueren"), "⛅": ("多云", "sun behind cloud duoyun"),
    "⛎": ("蛇夫座", "ophiuchus shefuzuo"), "⛔": ("禁止通行", "no entry jinzhitongxing"),
    "⛪": ("教堂", "church jiaotang"), "⛲": ("喷泉", "fountain penquan"),
    "⛳": ("高尔夫", "flag in hole gaoerfu"), "⛵": ("帆船", "sailboat fanchuan"),
    "⛺": ("帐篷", "tent zhangpeng"), "⛽": ("加油站", "fuel pump jiayouzhan"),
    "✁": ("剪上", "upper blade scissors jianshang"), "✃": ("剪下", "lower blade scissors jianxia"),
    "✄": ("白色剪刀", "white scissors baisejiandao"), "✅": ("勾选按钮", "check mark button gouxuan"),
    "❌": ("叉号按钮", "cross mark chahao"), "❎": ("叉号按钮", "cross mark button chahao"),
    "❓": ("问号", "question mark wenhao"), "❔": ("白色问号", "white question mark baise"),
    "❕": ("白色感叹号", "white exclamation mark baise"), "❗": ("感叹号", "exclamation mark gantanhao"),
    "❣": ("心形感叹号", "heart exclamation xinxing"), "❤": ("红心", "red heart hongxin"),
    "➿": ("双环", "double curly loop shuanghuan"), "〰": ("波浪线", "wavy dash bolangxian"),
    "〽": ("交替标记", "part alternation mark jiaoti"),
}

# Greek letters are common in formulas and unit names. Category 6.
GREEK = {
    "α": "alpha", "β": "beta", "γ": "gamma", "δ": "delta", "ε": "epsilon", "ζ": "zeta",
    "η": "eta", "θ": "theta", "ι": "iota", "κ": "kappa", "λ": "lambda", "μ": "mu",
    "ν": "nu", "ξ": "xi", "ο": "omicron", "π": "pi", "ρ": "rho", "ς": "final sigma",
    "σ": "sigma", "τ": "tau", "υ": "upsilon", "φ": "phi", "χ": "chi", "ψ": "psi", "ω": "omega",
    "Γ": "gamma", "Δ": "delta", "Θ": "theta", "Λ": "lambda", "Ξ": "xi", "Π": "pi",
    "Σ": "sigma", "Υ": "upsilon", "Φ": "phi", "Ψ": "psi", "Ω": "omega",
}
GREEK_ZH = {
    "alpha": "阿尔法", "beta": "贝塔", "gamma": "伽马", "delta": "德尔塔", "epsilon": "艾普西龙",
    "zeta": "泽塔", "eta": "伊塔", "theta": "西塔", "iota": "约塔", "kappa": "卡帕",
    "lambda": "拉姆达", "mu": "缪", "nu": "纽", "xi": "克西", "omicron": "奥米克戎",
    "pi": "派", "rho": "柔", "sigma": "西格玛", "final sigma": "西格玛", "tau": "陶",
    "upsilon": "宇普西龙", "phi": "斐", "chi": "卡伊", "psi": "普西", "omega": "欧米伽",
}

# Natural Unicode blocks per tab. Entries already added (common or curated)
# are skipped, so every character appears exactly once.
RANGES = {
    1: [(0x00A1, 0x00BF), (0x2000, 0x206F)],
    2: [(0x20A0, 0x20CF)],
    3: [(0x2200, 0x22FF)],
    4: [(0x2190, 0x21FF), (0x27F0, 0x27FF)],
    5: [(0x25A0, 0x25FF)],
    6: [(0x2070, 0x209C), (0x2100, 0x214F), (0x2150, 0x218F), (0x2460, 0x24EA)],
    7: [(0x2300, 0x23FF), (0x2600, 0x26FF), (0x2700, 0x27BF)],
}

# A few Latin-1 characters belong in the letter tab even though their block is
# punctuation.
CATEGORY_OVERRIDE = {0x00AA: 6, 0x00B2: 6, 0x00B3: 6, 0x00B9: 6, 0x00B5: 6,
                     0x00BA: 6, 0x00BC: 6, 0x00BD: 6, 0x00BE: 6,
                     0x00A2: 2, 0x00A3: 2, 0x00A4: 2, 0x00A5: 2}

SKIP_CATEGORIES = {"Cn", "Cc", "Cf", "Cs", "Co", "Mn", "Mc", "Me", "Zl", "Zp", "Zs"}
SKIP_CODEPOINTS = {0x24EB, 0x24EC, 0x24ED, 0x24EE, 0x24EF, 0x24F0, 0x24F1, 0x24F2, 0x24F3, 0x24F4}

FRACTIONS = {
    0x2150: "七分之一", 0x2151: "九分之一", 0x2152: "十分之一", 0x2153: "三分之一",
    0x2154: "三分之二", 0x2155: "五分之一", 0x2156: "五分之二", 0x2157: "五分之三",
    0x2158: "五分之四", 0x2159: "六分之一", 0x215A: "六分之五", 0x215B: "八分之一",
    0x215C: "八分之三", 0x215D: "八分之五", 0x215E: "八分之七", 0x215F: "分数 1",
}
NUMBER_WORDS = {"ZERO": "0", "ONE": "1", "TWO": "2", "THREE": "3", "FOUR": "4",
                "FIVE": "5", "SIX": "6", "SEVEN": "7", "EIGHT": "8", "NINE": "9"}


def auto_chinese(ch):
    """Chinese label for groups that follow a regular pattern."""
    cp = ord(ch)
    if 0x2460 <= cp <= 0x2473:
        return f"带圈数字 {cp-0x2460+1}"
    if cp == 0x24EA:
        return "带圈数字 0"
    if 0x2474 <= cp <= 0x2487:
        return f"括号数字 {cp-0x2474+1}"
    if 0x2488 <= cp <= 0x249B:
        return f"带点数字 {cp-0x2488+1}"
    if 0x249C <= cp <= 0x24B5:
        return "括号字母 " + chr(cp - 0x249C + ord("a"))
    if 0x24B6 <= cp <= 0x24CF:
        return "带圈字母 " + chr(cp - 0x24B6 + ord("A"))
    if 0x24D0 <= cp <= 0x24E9:
        return "带圈字母 " + chr(cp - 0x24D0 + ord("a"))
    if 0x2160 <= cp <= 0x217F:
        value = unicodedata.numeric(ch, 0)
        return f"罗马数字 {int(value)}" if value else None
    if cp in FRACTIONS:
        return FRACTIONS[cp]
    name = unicodedata.name(ch, "")
    if 0x2070 <= cp <= 0x209C:
        if name.startswith("SUPERSCRIPT "):
            label = name[len("SUPERSCRIPT "):]
            prefix = "上标 "
        elif "SUBSCRIPT " in name:
            label = name.split("SUBSCRIPT ", 1)[1]
            prefix = "下标 "
        else:
            return None
        label = NUMBER_WORDS.get(label, label.replace("LATIN SMALL LETTER ", "")
                                 .replace("LATIN CAPITAL LETTER ", "").replace("SMALL LETTER ", "").replace("CAPITAL LETTER ", ""))
        if len(label) == 1 and label.isalpha():
            label = label.lower()
        for word, symbol in [("PLUS SIGN", "+"), ("MINUS SIGN", "−"), ("EQUALS SIGN", "="),
                             ("LEFT PARENTHESIS", "("), ("RIGHT PARENTHESIS", ")")]:
            if label == word:
                label = symbol
        return prefix + label
    return None


def pinyin_index(names):
    try:
        result = subprocess.run(["uconv", "-x", "Han-Latin; Latin-ASCII; Lower()"],
                                input="\n".join(names), text=True, capture_output=True, check=True)
        lines = result.stdout.splitlines()
        if len(lines) == len(names):
            return lines
    except (FileNotFoundError, subprocess.CalledProcessError):
        pass
    return [""] * len(names)


def main():
    entries = {}   # codepoint -> dict, preserves insertion order

    def add(ch, category, zh, keywords):
        cp = ord(ch)
        if cp in entries:
            return
        name = unicodedata.name(ch, "")
        entries[cp] = {
            "text": ch,
            "name": zh or name.lower(),
            "english": name.lower() if zh else f"U+{cp:04X}",
            "category": category,
            "code": f"U+{cp:04X}",
            "keywords": keywords,
        }

    for ch, zh, keywords in COMMON:
        add(ch, 0, zh, keywords)

    def natural_category(cp):
        if cp in CATEGORY_OVERRIDE:
            return CATEGORY_OVERRIDE[cp]
        for category, ranges in RANGES.items():
            if any(start <= cp <= end for start, end in ranges):
                return category
        return None

    for ch, (zh, keywords) in CURATED.items():
        add(ch, natural_category(ord(ch)) or 7, zh, keywords)

    for ch, name in GREEK.items():
        cp = ord(ch)
        if cp in entries:
            continue
        zh = GREEK_ZH[name]
        add(ch, 6, zh, f"greek letter {name} 希腊字母")

    for category, ranges in RANGES.items():
        for start, end in ranges:
            for cp in range(start, end + 1):
                ch = chr(cp)
                if cp in SKIP_CODEPOINTS:
                    continue
                if unicodedata.category(ch) in SKIP_CATEGORIES:
                    continue
                if not unicodedata.name(ch, ""):
                    continue
                target = CATEGORY_OVERRIDE.get(cp, category)
                zh = auto_chinese(ch)
                curated = CURATED.get(ch)
                if curated:
                    zh, keywords = curated
                else:
                    keywords = ""
                add(ch, target, zh, keywords)

    names = [entry["name"] for entry in entries.values()]
    pinyins = pinyin_index(names)
    result = []
    for (cp, entry), pinyin in zip(entries.items(), pinyins):
        category_name = CATEGORIES[entry["category"]]
        text = entry["text"]
        words = [entry["english"], entry["name"], pinyin, category_name,
                 "symbol 符号", entry["code"], text]
        if entry["keywords"]:
            words.append(entry["keywords"])
        # Search-only keywords, including compact pinyin and hex without the U+.
        aliases = []
        for word in " ".join(words).split():
            aliases.append(word)
        for token in pinyin.split():
            if token:
                aliases.append(token)
        compact = "".join(pinyin.split())
        if compact:
            aliases.append(compact)
        aliases.append(entry["code"][2:].lower())
        combined = " ".join(dict.fromkeys(words))
        entry.update(pinyin=pinyin, keywords=combined, aliases=list(dict.fromkeys(aliases)))
        result.append(entry)

    catalogue = {"unicode": unicodedata.unidata_version, "categories": CATEGORIES, "entries": result}
    output = ROOT / "data/symbols.json"
    output.write_text(json.dumps(catalogue, ensure_ascii=False, separators=(",", ":")) + "\n")
    print(f"Generated {len(result)} symbols in {len(CATEGORIES)} categories -> {output.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
