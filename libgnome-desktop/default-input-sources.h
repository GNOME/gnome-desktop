typedef struct
{
  const gchar *const locale;
  const gchar *const type;
  const gchar *const id;
} DefaultInputSource;

static DefaultInputSource default_input_sources[] =
{
  { "ar_DZ",    "xkb",          "ara+azerty" },
  { "as_IN",    "ibus",         "m17n:as:phonetic" },
  { "ast_ES",   "xkb",          "es+ast" },
  { "az_AZ",    "xkb",          "az" },
  { "be_BY",    "xkb",          "by" },
  { "bg_BG",    "xkb",          "bg+phonetic" },
  { "bn_IN",    "ibus",         "m17n:bn:inscript" },
  { "cat_ES",   "xkb",          "es+cat" },
  { "cs_CZ",    "xkb",          "cz" },
  { "de_CH",    "xkb",          "ch" },
  { "de_DE",    "xkb",          "de" },
  { "el_CY",    "xkb",          "gr" },
  { "el_GR",    "xkb",          "gr" },
  { "en_GB",    "xkb",          "gb" },
  { "en_US",    "xkb",          "us" },
  { "en_ZA",    "xkb",          "za" },
  { "es_ES",    "xkb",          "es" },
  { "es_GT",    "xkb",          "latam" },
  { "es_MX",    "xkb",          "latam" },
  { "es_US",    "xkb",          "us+intl" },
  { "fr_BE",    "xkb",          "be" },
  { "fr_CH",    "xkb",          "ch+fr" },
  { "fr_FR",    "xkb",          "fr+oss" },
  { "gl_ES",    "xkb",          "es" },
  { "gu_IN",    "ibus",         "m17n:gu:inscript" },
  { "he_IL",    "xkb",          "il" },
  { "hi_IN",    "ibus",         "m17n:hi:inscript" },
  { "id_ID",    "xkb",          "us" },
  { "it_IT",    "xkb",          "it" },
  { "ja_JP",    "ibus",         "anthy" },
  { "kk_KZ",    "xkb",          "kz" },
  { "kn_IN",    "ibus",         "m17n:kn:kgp" },
  { "ko_KR",    "ibus",         "hangul" },
  { "mai_IN",   "ibus",         "m17n:mai:inscript" },
  { "mk_MK",    "xkb",          "mk" },
  { "ml_IN",    "ibus",         "m17n:ml:inscript" },
  { "mr_IN",    "ibus",         "m17n:mr:inscript" },
  { "nl_NL",    "xkb",          "us+altgr-intl" },
  { "or_IN",    "ibus",         "m17n:or:inscript" },
  { "pa_IN",    "ibus",         "m17n:pa:inscript" },
  { "pl_PL",    "xkb",          "pl" },
  { "pt_BR",    "xkb",          "br" },
  { "pt_PT",    "xkb",          "pt" },
  { "ru_RU",    "xkb",          "ru" },
  { "sd_IN",    "ibus",         "m17n:sd:inscript" },
  { "sk_SK",    "xkb",          "sk" },
  { "ta_IN",    "ibus",         "m17n:ta:tamil99" },
  { "te_IN",    "ibus",         "m17n:te:inscript" },
  { "tr_TR",    "xkb",          "tr" },
  { "ur_IN",    "ibus",         "m17n:ur:phonetic" },
  { "uk_UA",    "xkb",          "ua" },
  { "zh_CN",    "ibus",         "libpinyin" },
  { "zh_HK",    "ibus",         "table:cangjie5" },
  { "zh_TW",    "ibus",         "libzhuyin" },
  { NULL,       NULL,           NULL }
};

const char * const *non_latin_input_sources[] = { "xkb+bg", "xkb+by", "xkb+cz", "xkb+gr", "xkb+kz", "xkb+mk", "xkb+ru", "xkb+ua", NULL };
