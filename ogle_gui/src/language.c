/* Ogle - A video player
 * Copyright (C) 2000, 2001 Vilhelm Bergman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include <ogle/dvdcontrol.h>
#include "language.h"

langcodes_t language[] = {
  { N_("Abkhazian"), "abk", "abk", "ab"},
  { N_("Achinese"), "ace", "ace", ""},
  { N_("Acoli"), "ach", "ach", ""},
  { N_("Adangme"), "ada", "ada", ""},
  { N_("Afar"), "aar", "aar", "aa"},
  { N_("Afrihili"), "afh", "afh", ""},
  { N_("Afrikaans"), "afr", "afr", "af"},
  { N_("Afro-Asiatic (Other)"), "afa", "afa", ""},
  { N_("Akan"), "aka", "aka", ""},
  { N_("Akkadian"), "akk", "akk", ""},
  { N_("Albanian"), "alb", "sqi", "sq"},
  { N_("Aleut"), "ale", "ale", ""},
  { N_("Algonquian languages"), "alg", "alg", ""},
  { N_("Altaic (Other)"), "tut", "tut", ""},
  { N_("Amharic"), "amh", "amh", "am"},
  { N_("Apache languages"), "apa", "apa", ""},
  { N_("Arabic"), "ara", "ara", "ar"},
  { N_("Aramaic"), "arc", "arc", ""},
  { N_("Arapaho"), "arp", "arp", ""},
  { N_("Araucanian"), "arn", "arn", ""},
  { N_("Arawak"), "arw", "arw", ""},
  { N_("Armenian"), "arm", "hye", "hy"},
  { N_("Artificial (Other)"), "art", "art", ""},
  { N_("Assamese"), "asm", "asm", "as"},
  { N_("Athapascan languages"), "ath", "ath", ""},
  { N_("Australian languages"), "aus", "aus", ""},
  { N_("Austronesian (Other)"), "map", "map", ""},
  { N_("Avaric"), "ava", "ava", ""},
  { N_("Avestan"), "ave", "ave", "ae"},
  { N_("Awadhi"), "awa", "awa", ""},
  { N_("Aymara"), "aym", "aym", "ay"},
  { N_("Azerbaijani"), "aze", "aze", "az"},
  { N_("Balinese"), "ban", "ban", ""},
  { N_("Baltic (Other)"), "bat", "bat", ""},
  { N_("Baluchi"), "bal", "bal", ""},
  { N_("Bambara"), "bam", "bam", ""},
  { N_("Bamileke languages"), "bai", "bai", ""},
  { N_("Banda"), "bad", "bad", ""},
  { N_("Bantu (Other)"), "bnt", "bnt", ""},
  { N_("Basa"), "bas", "bas", ""},
  { N_("Bashkir"), "bak", "bak", "ba"},
  { N_("Basque"), "baq", "eus", "eu"},
  { N_("Batak (Indonesia)"), "btk", "btk", ""},
  { N_("Beja"), "bej", "bej", ""},
  { N_("Belarusian"), "bel", "bel", "be"},
  { N_("Bemba"), "bem", "bem", ""},
  { N_("Bengali"), "ben", "ben", "bn"},
  { N_("Berber (Other)"), "ber", "ber", ""},
  { N_("Bhojpuri"), "bho", "bho", ""},
  { N_("Bihari"), "bih", "bih", "bh"},
  { N_("Bikol"), "bik", "bik", ""},
  { N_("Bini"), "bin", "bin", ""},
  { N_("Bislama"), "bis", "bis", "bi"},
  { N_("Bosnian"), "bos", "bos", "bs"},
  { N_("Braj"), "bra", "bra", ""},
  { N_("Breton"), "bre", "bre", "br"},
  { N_("Buginese"), "bug", "bug", ""},
  { N_("Bulgarian"), "bul", "bul", "bg"},
  { N_("Buriat"), "bua", "bua", ""},
  { N_("Burmese"), "bur", "mya", "my"},
  { N_("Caddo"), "cad", "cad", ""},
  { N_("Carib"), "car", "car", ""},
  { N_("Catalan"), "cat", "cat", "ca"},
  { N_("Caucasian (Other)"), "cau", "cau", ""},
  { N_("Cebuano"), "ceb", "ceb", ""},
  { N_("Celtic (Other)"), "cel", "cel", ""},
  { N_("Central American Indian (Other)"), "cai", "cai", ""},
  { N_("Chagatai"), "chg", "chg", ""},
  { N_("Chamic languages"), "cmc", "cmc", ""},
  { N_("Chamorro"), "cha", "cha", "ch"},
  { N_("Chechen"), "che", "che", "ce"},
  { N_("Cherokee"), "chr", "chr", ""},
  { N_("Cheyenne"), "chy", "chy", ""},
  { N_("Chibcha"), "chb", "chb", ""},
  { N_("Chichewa; Nyanja"), "nya", "nya", "ny"},
  { N_("Chinese"), "chi", "zho", "zh"},
  { N_("Chinook jargon"), "chn", "chn", ""},
  { N_("Chipewyan"), "chp", "chp", ""},
  { N_("Choctaw"), "cho", "cho", ""},
  { N_("Church Slavic"), "chu", "chu", "cu"},
  { N_("Chuukese"), "chk", "chk", ""},
  { N_("Chuvash"), "chv", "chv", "cv"},
  { N_("Coptic"), "cop", "cop", ""},
  { N_("Cornish"), "cor", "cor", "kw"},
  { N_("Corsican"), "cos", "cos", "co"},
  { N_("Cree"), "cre", "cre", ""},
  { N_("Creek"), "mus", "mus", ""},
  { N_("Creoles and pidgins (Other)"), "crp", "crp", ""},
  { N_("Creoles and pidgins, English-based (Other)"), "cpe", "cpe", ""},
  { N_("Creoles and pidgins, French-based (Other)"),  "cpf", "cpf", ""},
  { N_("Creoles and pidgins, Portuguese-based (Other)"), "cpp", "cpp", ""},
  { N_("Croatian"), "scr", "hrv", "hr"},
  { N_("Cushitic (Other)"), "cus", "cus", ""},
  { N_("Czech"), "cze", "ces", "cs"},
  { N_("Dakota"), "dak", "dak", ""},
  { N_("Danish"), "dan", "dan", "da"},
  { N_("Dayak"), "day", "day", ""},
  { N_("Delaware"), "del", "del", ""},
  { N_("Dinka"), "din", "din", ""},
  { N_("Divehi"), "div", "div", ""},
  { N_("Dogri"), "doi", "doi", ""},
  { N_("Dogrib"), "dgr", "dgr", ""},
  { N_("Dravidian (Other)"), "dra", "dra", ""},
  { N_("Duala"), "dua", "dua", ""},
  { N_("Dutch"), "dut", "nld", "nl"},
  { N_("Dutch, Middle(ca. 1050-1350)"), "dum", "dum", ""},
  { N_("Dyula"), "dyu", "dyu", ""},
  { N_("Dzongkha"), "dzo", "dzo", "dz"},
  { N_("Efik"), "efi", "efi", ""},
  { N_("Egyptian (Ancient)"), "egy", "egy", ""},
  { N_("Ekajuk"), "eka", "eka", ""},
  { N_("Elamite"), "elx", "elx", ""},
  { N_("English"), "eng", "eng", "en"},
  { N_("English, Middle (1100-1500)"), "enm", "enm", ""},
  { N_("English, Old (ca.450-1100)"), "ang", "ang", ""},
  { N_("Esperanto"), "epo", "epo", "eo"},
  { N_("Estonian"), "est", "est", "et"},
  { N_("Ewe"), "ewe", "ewe", ""},
  { N_("Ewondo"), "ewo", "ewo", ""},
  { N_("Fang"), "fan", "fan", ""},
  { N_("Fanti"), "fat", "fat", ""},
  { N_("Faroese"), "fao", "fao", "fo"},
  { N_("Fijian"), "fij", "fij", "fj"},
  { N_("Finnish"), "fin", "fin", "fi"},
  { N_("Finno-Ugrian (Other)"), "fiu", "fiu", ""},
  { N_("Fon"), "fon", "fon", ""},
  { N_("French"), "fre", "fra", "fr"},
  { N_("French, Middle (ca.1400-1600)"), "frm", "frm", ""},
  { N_("French, Old (842-ca.1400)"), "fro", "fro", ""},
  { N_("Frisian"), "fry", "fry", "fy"},
  { N_("Friulian"), "fur", "fur", ""},
  { N_("Fulah"), "ful", "ful", ""},
  { N_("Ga"), "gaa", "gaa", ""},
  { N_("Gaelic (Scots)"), "gla", "gla", "gd"},
  { N_("Gallegan"), "glg", "glg", "gl"},
  { N_("Ganda"), "lug", "lug", ""},
  { N_("Gayo"), "gay", "gay", ""},
  { N_("Gbaya"), "gba", "gba", ""},
  { N_("Geez"), "gez", "gez", ""},
  { N_("Georgian"), "geo", "kat", "ka"},
  { N_("German"), "ger", "deu", "de"},
  { N_("German, Low; Saxon, Low; Low German; Low Saxon"), "nds", "nds", ""},
  { N_("German, Middle High (ca.1050-1500)"), "gmh", "gmh", ""},
  { N_("German, Old High (ca.750-1050)"), "goh", "goh", ""},
  { N_("Germanic (Other)"), "gem", "gem", ""},
  { N_("Gilbertese"), "gil", "gil", ""},
  { N_("Gondi"), "gon", "gon", ""},
  { N_("Gorontalo"), "gor", "gor", ""},
  { N_("Gothic"), "got", "got", ""},
  { N_("Grebo"), "grb", "grb", ""},
  { N_("Greek, Ancient (to 1453)"), "grc", "grc", ""},
  { N_("Greek, Modern (1453-)"), "gre", "ell", "el"},
  { N_("Guarani"), "grn", "grn", "gn"},
  { N_("Gujarati"), "guj", "guj", "gu"},
  { N_("Gwich&#180;in"), "gwi", "gwi", ""},
  { N_("Haida"), "hai", "hai", ""},
  { N_("Hausa"), "hau", "hau", "ha"},
  { N_("Hawaiian"), "haw", "haw", ""},
  { N_("Hebrew"), "heb", "heb", "he"},
  { N_("Herero"), "her", "her", "hz"},
  { N_("Hiligaynon"), "hil", "hil", ""},
  { N_("Himachali"), "him", "him", ""},
  { N_("Hindi"), "hin", "hin", "hi"},
  { N_("Hiri Motu"), "hmo", "hmo", "ho"},
  { N_("Hittite"), "hit", "hit", ""},
  { N_("Hmong"), "hmn", "hmn", ""},
  { N_("Hungarian"), "hun", "hun", "hu"},
  { N_("Hupa"), "hup", "hup", ""},
  { N_("Iban"), "iba", "iba", ""},
  { N_("Icelandic"), "ice", "isl", "is"},
  { N_("Igbo"), "ibo", "ibo", ""},
  { N_("Ijo"), "ijo", "ijo", ""},
  { N_("Iloko"), "ilo", "ilo", ""},
  { N_("Indic (Other)"), "inc", "inc", ""},
  { N_("Indo-European (Other)"), "ine", "ine", ""},
  { N_("Indonesian"), "ind", "ind", "id"},
  { N_("Interlingua (International Auxiliary Language Association)"), "ina", "ina", "ia"},
  { N_("Interlingue"), "ile", "ile", "ie"},
  { N_("Inuktitut"), "iku", "iku", "iu"},
  { N_("Inupiaq"), "ipk", "ipk", "ik"},
  { N_("Iranian (Other)"), "ira", "ira", ""},
  { N_("Irish"), "gle", "gle", "ga"},
  { N_("Irish, Middle (900-1200)"), "mga", "mga", ""},
  { N_("Irish, Old (to 900)"), "sga", "sga", ""},
  { N_("Iroquoian languages"), "iro", "iro", ""},
  { N_("Italian"), "ita", "ita", "it"},
  { N_("Japanese"), "jpn", "jpn", "ja"},
  { N_("Javanese"), "jav", "jaw", "jw"},
  { N_("Judeo-Arabic"), "jrb", "jrb", ""},
  { N_("Judeo-Persian"), "jpr", "jpr", ""},
  { N_("Kabyle"), "kab", "kab", ""},
  { N_("Kachin"), "kac", "kac", ""},
  { N_("Kalaallisut"), "kal", "kal", "kl"},
  { N_("Kamba"), "kam", "kam", ""},
  { N_("Kannada"), "kan", "kan", "kn"},
  { N_("Kanuri"), "kau", "kau", ""},
  { N_("Kara-Kalpak"), "kaa", "kaa", ""},
  { N_("Karen"), "kar", "kar", ""},
  { N_("Kashmiri"), "kas", "kas", "ks"},
  { N_("Kawi"), "kaw", "kaw", ""},
  { N_("Kazakh"), "kaz", "kaz", "kk"},
  { N_("Khasi"), "kha", "kha", ""},
  { N_("Khmer"), "khm", "khm", "km"},
  { N_("Khoisan (Other)"), "khi", "khi", ""},
  { N_("Khotanese"), "kho", "kho", ""},
  { N_("Kikuyu"), "kik", "kik", "ki"},
  { N_("Kimbundu"), "kmb", "kmb", ""},
  { N_("Kinyarwanda"), "kin", "kin", "rw"},
  { N_("Kirghiz"), "kir", "kir", "ky"},
  { N_("Komi"), "kom", "kom", "kv"},
  { N_("Kongo"), "kon", "kon", ""},
  { N_("Konkani"), "kok", "kok", ""},
  { N_("Korean"), "kor", "kor", "ko"},
  { N_("Kosraean"), "kos", "kos", ""},
  { N_("Kpelle"), "kpe", "kpe", ""},
  { N_("Kru"), "kro", "kro", ""},
  { N_("Kuanyama"), "kua", "kua", "kj"},
  { N_("Kumyk"), "kum", "kum", ""},
  { N_("Kurdish"), "kur", "kur", "ku"},
  { N_("Kurukh"), "kru", "kru", ""},
  { N_("Kutenai"), "kut", "kut", ""},
  { N_("Ladino"), "lad", "lad", ""},
  { N_("Lahnda"), "lah", "lah", ""},
  { N_("Lamba"), "lam", "lam", ""},
  { N_("Lao"), "lao", "lao", "lo"},
  { N_("Latin"), "lat", "lat", "la"},
  { N_("Latvian"), "lav", "lav", "lv"},
  { N_("Letzeburgesch"), "ltz", "ltz", "lb"},
  { N_("Lezghian"), "lez", "lez", ""},
  { N_("Lingala"), "lin", "lin", "ln"},
  { N_("Lithuanian"), "lit", "lit", "lt"},
  { N_("Low German; Low Saxon; German, Low; Saxon, Low"), "nds", "nds", ""},
  { N_("Low Saxon; Low German; Saxon, Low; German, Low"), "nds", "nds", ""},
  { N_("Lozi"), "loz", "loz", ""},
  { N_("Luba-Katanga"), "lub", "lub", ""},
  { N_("Luba-Lulua"), "lua", "lua", ""},
  { N_("Luiseno"), "lui", "lui", ""},
  { N_("Lunda"), "lun", "lun", ""},
  { N_("Luo (Kenya and Tanzania)"), "luo", "luo", ""},
  { N_("Lushai"), "lus", "lus", ""},
  { N_("Macedonian"), "mac", "mkd", "mk"},
  { N_("Madurese"), "mad", "mad", ""},
  { N_("Magahi"), "mag", "mag", ""},
  { N_("Maithili"), "mai", "mai", ""},
  { N_("Makasar"), "mak", "mak", ""},
  { N_("Malagasy"), "mlg", "mlg", "mg"},
  { N_("Malay"), "may", "msa", "ms"},
  { N_("Malayalam"), "mal", "mal", "ml"},
  { N_("Maltese"), "mlt", "mlt", "mt"},
  { N_("Manchu"), "mnc", "mnc", ""},
  { N_("Mandar"), "mdr", "mdr", ""},
  { N_("Mandingo"), "man", "man", ""},
  { N_("Manipuri"), "mni", "mni", ""},
  { N_("Manobo languages"), "mno", "mno", ""},
  { N_("Manx"), "glv", "glv", "gv"},
  { N_("Maori"), "mao", "mri", "mi"},
  { N_("Marathi"), "mar", "mar", "mr"},
  { N_("Mari"), "chm", "chm", ""},
  { N_("Marshall"), "mah", "mah", "mh"},
  { N_("Marwari"), "mwr", "mwr", ""},
  { N_("Masai"), "mas", "mas", ""},
  { N_("Mayan languages"), "myn", "myn", ""},
  { N_("Mende"), "men", "men", ""},
  { N_("Micmac"), "mic", "mic", ""},
  { N_("Minangkabau"), "min", "min", ""},
  { N_("Miscellaneous languages"), "mis", "mis", ""},
  { N_("Mohawk"), "moh", "moh", ""},
  { N_("Moldavian"), "mol", "mol", "mo"},
  { N_("Mon-Khmer (Other)"), "mkh", "mkh", ""},
  { N_("Mongo"), "lol", "lol", ""},
  { N_("Mongolian"), "mon", "mon", "mn"},
  { N_("Mossi"), "mos", "mos", ""},
  { N_("Multiple languages"), "mul", "mul", ""},
  { N_("Munda languages"), "mun", "mun", ""},
  { N_("Nahuatl"), "nah", "nah", ""},
  { N_("Nauru"), "nau", "nau", "na"},
  { N_("Navajo"), "nav", "nav", "nv"},
  { N_("Ndebele, North"), "nde", "nde", "nd"},
  { N_("Ndebele, South"), "nbl", "nbl", "nr"},
  { N_("Ndonga"), "ndo", "ndo", "ng"},
  { N_("Nepali"), "nep", "nep", "ne"},
  { N_("Newari"), "new", "new", ""},
  { N_("Nias"), "nia", "nia", ""},
  { N_("Niger-Kordofanian (Other)"), "nic", "nic", ""},
  { N_("Nilo-Saharan (Other)"), "ssa", "ssa", ""},
  { N_("Niuean"), "niu", "niu", ""},
  { N_("Norse, Old"), "non", "non", ""},
  { N_("North American Indian (Other)"), "nai", "nai", ""},
  { N_("Northern Sami"), "sme", "sme", "se"},
  { N_("Norwegian"), "nor", "nor", "no"},
  { N_("Norwegian Bokm&aring;l"), "nob", "nob", "nb"},
  { N_("Norwegian Nynorsk"), "nno", "nno", "nn"},
  { N_("Nubian languages"), "nub", "nub", ""},
  { N_("Nyamwezi"), "nym", "nym", ""},
  { N_("Nyanja; Chichewa"), "nya", "nya", "ny"},
  { N_("Nyankole"), "nyn", "nyn", ""},
  { N_("Nyoro"), "nyo", "nyo", ""},
  { N_("Nzima"), "nzi", "nzi", ""},
  { N_("Occitan (post 1500); Proven&ccedil;al"), "oci", "oci", "oc"},
  { N_("Ojibwa"), "oji", "oji", ""},
  { N_("Oriya"), "ori", "ori", "or"},
  { N_("Oromo"), "orm", "orm", "om"},
  { N_("Osage"), "osa", "osa", ""},
  { N_("Ossetian; Ossetic"), "oss", "oss", "os"},
  { N_("Ossetic; Ossetian"), "oss", "oss", "os"},
  { N_("Otomian languages"), "oto", "oto", ""},
  { N_("Pahlavi"), "pal", "pal", ""},
  { N_("Palauan"), "pau", "pau", ""},
  { N_("Pali"), "pli", "pli", "pi"},
  { N_("Pampanga"), "pam", "pam", ""},
  { N_("Pangasinan"), "pag", "pag", ""},
  { N_("Panjabi"), "pan", "pan", "pa"},
  { N_("Papiamento"), "pap", "pap", ""},
  { N_("Papuan (Other)"), "paa", "paa", ""},
  { N_("Persian"), "per", "fas", "fa"},
  { N_("Persian, Old (ca.600-400 B.C.)"), "peo", "peo", ""},
  { N_("Philippine (Other)"), "phi", "phi", ""},
  { N_("Phoenician"), "phn", "phn", ""},
  { N_("Pohnpeian"), "pon", "pon", ""},
  { N_("Polish"), "pol", "pol", "pl"},
  { N_("Portuguese"), "por", "por", "pt"},
  { N_("Prakrit languages"), "pra", "pra", ""},
  { N_("Proven&#231;al; Occitan (post 1500) "), "oci", "oci", "oc"},
  { N_("Proven&#231;al, Old (to 1500)"), "pro", "pro", ""},
  { N_("Pushto"), "pus", "pus", "ps"},
  { N_("Quechua"), "que", "que", "qu"},
  { N_("Raeto-Romance"), "roh", "roh", "rm"},
  { N_("Rajasthani"), "raj", "raj", ""},
  { N_("Rapanui"), "rap", "rap", ""},
  { N_("Rarotongan"), "rar", "rar", ""},
  //{ N_("Reserved for local use"), "qaa-qtz", "qaa-qtz", ""},
  { N_("Romance (Other)"), "roa", "roa", ""},
  { N_("Romanian"), "rum", "ron", "ro"},
  { N_("Romany"), "rom", "rom", ""},
  { N_("Rundi"), "run", "run", "rn"},
  { N_("Russian"), "rus", "rus", "ru"},
  { N_("Salishan languages"), "sal", "sal", ""},
  { N_("Samaritan Aramaic"), "sam", "sam", ""},
  { N_("Sami languages (Other)"), "smi", "smi", ""},
  { N_("Samoan"), "smo", "smo", "sm"},
  { N_("Sandawe"), "sad", "sad", ""},
  { N_("Sango"), "sag", "sag", "sg"},
  { N_("Sanskrit"), "san", "san", "sa"},
  { N_("Santali"), "sat", "sat", ""},
  { N_("Sardinian"), "srd", "srd", "sc"},
  { N_("Sasak"), "sas", "sas", ""},
  { N_("Saxon, Low; German, Low;  Low Saxon; Low German"), "nds", "nds", ""},
  { N_("Scots"), "sco", "sco", ""},
  { N_("Selkup"), "sel", "sel", ""},
  { N_("Semitic (Other)"), "sem", "sem", ""},
  { N_("Serbian"), "scc", "srp", "sr"},
  { N_("Serer"), "srr", "srr", ""},
  { N_("Shan"), "shn", "shn", ""},
  { N_("Shona"), "sna", "sna", "sn"},
  { N_("Sidamo"), "sid", "sid", ""},
  { N_("Sign languages"), "sgn", "sgn", ""},
  { N_("Siksika"), "bla", "bla", ""},
  { N_("Sindhi"), "snd", "snd", "sd"},
  { N_("Sinhalese"), "sin", "sin", "si"},
  { N_("Sino-Tibetan (Other)"), "sit", "sit", ""},
  { N_("Siouan languages"), "sio", "sio", ""},
  { N_("Slave (Athapascan)"), "den", "den", ""},
  { N_("Slavic (Other)"), "sla", "sla", ""},
  { N_("Slovak"), "slo", "slk", "sk"},
  { N_("Slovenian"), "slv", "slv", "sl"},
  { N_("Sogdian"), "sog", "sog", ""},
  { N_("Somali"), "som", "som", "so"},
  { N_("Songhai"), "son", "son", ""},
  { N_("Soninke"), "snk", "snk", ""},
  { N_("Sorbian languages"), "wen", "wen", ""},
  { N_("Sotho, Northern"), "nso", "nso", ""},
  { N_("Sotho, Southern"), "sot", "sot", "st"},
  { N_("South American Indian (Other)"), "sai", "sai", ""},
  { N_("Spanish"), "spa", "spa", "es"},
  { N_("Sukuma"), "suk", "suk", ""},
  { N_("Sumerian"), "sux", "sux", ""},
  { N_("Sundanese"), "sun", "sun", "su"},
  { N_("Susu"), "sus", "sus", ""},
  { N_("Swahili"), "swa", "swa", "sw"},
  { N_("Swati"), "ssw", "ssw", "ss"},
  { N_("Swedish"), "swe", "swe", "sv"},
  { N_("Syriac"), "syr", "syr", ""},
  { N_("Tagalog"), "tgl", "tgl", "tl"},
  { N_("Tahitian"), "tah", "tah", "ty"},
  { N_("Tai (Other)"), "tai", "tai", ""},
  { N_("Tajik"), "tgk", "tgk", "tg"},
  { N_("Tamashek"), "tmh", "tmh", ""},
  { N_("Tamil"), "tam", "tam", "ta"},
  { N_("Tatar"), "tat", "tat", "tt"},
  { N_("Telugu"), "tel", "tel", "te"},
  { N_("Tereno"), "ter", "ter", ""},
  { N_("Tetum"), "tet", "tet", ""},
  { N_("Thai"), "tha", "tha", "th"},
  { N_("Tibetan"), "tib", "bod", "bo"},
  { N_("Tigre"), "tig", "tig", ""},
  { N_("Tigrinya"), "tir", "tir", "ti"},
  { N_("Timne"), "tem", "tem", ""},
  { N_("Tiv"), "tiv", "tiv", ""},
  { N_("Tlingit"), "tli", "tli", ""},
  { N_("Tok Pisin"), "tpi", "tpi", ""},
  { N_("Tokelau"), "tkl", "tkl", ""},
  { N_("Tonga (Nyasa)"), "tog", "tog", ""},
  { N_("Tonga (Tonga Islands)"), "ton", "ton", "to"},
  { N_("Tsimshian"), "tsi", "tsi", ""},
  { N_("Tsonga"), "tso", "tso", "ts"},
  { N_("Tswana"), "tsn", "tsn", "tn"},
  { N_("Tumbuka"), "tum", "tum", ""},
  { N_("Turkish"), "tur", "tur", "tr"},
  { N_("Turkish, Ottoman (1500-1928)"), "ota", "ota", ""},
  { N_("Turkmen"), "tuk", "tuk", "tk"},
  { N_("Tuvalu"), "tvl", "tvl", ""},
  { N_("Tuvinian"), "tyv", "tyv", ""},
  { N_("Twi"), "twi", "twi", "tw"},
  { N_("Ugaritic"), "uga", "uga", ""},
  { N_("Uighur"), "uig", "uig", "ug"},
  { N_("Ukrainian"), "ukr", "ukr", "uk"},
  { N_("Umbundu"), "umb", "umb", ""},
  { N_("Undetermined"), "und", "und", ""},
  { N_("Urdu"), "urd", "urd", "ur"},
  { N_("Uzbek"), "uzb", "uzb", "uz"},
  { N_("Vai"), "vai", "vai", ""},
  { N_("Venda"), "ven", "ven", ""},
  { N_("Vietnamese"), "vie", "vie", "vi"},
  { N_("Volap&#252;k"), "vol", "vol", "vo"},
  { N_("Votic"), "vot", "vot", ""},
  { N_("Wakashan languages"), "wak", "wak", ""},
  { N_("Walamo"), "wal", "wal", ""},
  { N_("Waray"), "war", "war", ""},
  { N_("Washo"), "was", "was", ""},
  { N_("Welsh"), "wel", "cym", "cy"},
  { N_("Wolof"), "wol", "wol", "wo"},
  { N_("Xhosa"), "xho", "xho", "xh"},
  { N_("Yakut"), "sah", "sah", ""},
  { N_("Yao"), "yao", "yao", ""},
  { N_("Yapese"), "yap", "yap", ""},
  { N_("Yiddish"), "yid", "yid", "yi"},
  { N_("Yoruba"), "yor", "yor", "yo"},
  { N_("Yupik languages"), "ypk", "ypk", ""},
  { N_("Zande"), "znd", "znd", ""},
  { N_("Zapotec"), "zap", "zap", ""},
  { N_("Zenaga"), "zen", "zen", ""},
  { N_("Zhuang"), "zha", "zha", "za"},
  { N_("Zulu"), "zul", "zul", "zu"},
  { N_("Zuni"), "zun", "zun", ""},
  { NULL,"","",""},
};


/**
 * Returns the localized language name by a DVDLangID_t
 */
char* language_name(DVDLangID_t lang) {
  int i = 0;
  char lang_code[2];
  
  lang_code[0] = lang >> 8;
  lang_code[1] = lang & 0xff;
  while(language[i].name != NULL) {
    if((language[i].code_639_1[0] != '\0')
       && (memcmp(&lang_code, &language[i].code_639_1[0], 2) == 0))
      return _(language[i].name);
    i++;
  }
  return N_("Unknown");
}

/**
 * Returns the iso-639-1 language code by a DVDLangID_t 
 */
char* language_code(DVDLangID_t lang) {
  int i = 0;
  char lang_code[2];
  
  lang_code[0] = lang >> 8;
  lang_code[1] = lang & 0xff;
  while(language[i].name !=NULL) {
    if((language[i].code_639_1[0] != '\0')
       && (memcmp(&lang_code, &language[i].code_639_1[0], 2) == 0))
      return language[i].code_639_1;
    i++;
  }
  return "??";
}

