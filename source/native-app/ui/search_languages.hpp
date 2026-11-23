// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string_view>
#include <iterator>
namespace sx::search_keyboard {
struct Language { const char* code; const char* name; std::u32string_view lower,upper; };
inline constexpr Language languages[]={
 {"en","English",U"qwertyuiopasdfghjklzxcvbnm",U"QWERTYUIOPASDFGHJKLZXCVBNM"},
 {"de","Deutsch",U"qwertzuiopüasdfghjklöäyxcvbnmß",U"QWERTZUIOPÜASDFGHJKLÖÄYXCVBNMẞ"},
 {"es","Español",U"qwertyuiopasdfghjklñzxcvbnmáéíóúü¿¡",U"QWERTYUIOPASDFGHJKLÑZXCVBNMÁÉÍÓÚÜ¿¡"},
 {"fr","Français",U"azertyuiopqsdfghjklmwxcvbnéèêëàâçîïôùûüœ",U"AZERTYUIOPQSDFGHJKLMWXCVBNÉÈÊËÀÂÇÎÏÔÙÛÜŒ"},
 {"he","עברית",U"קראטוןםפשדגכעיחלךףזסבהנמצתץ",U"קראטוןםפשדגכעיחלךףזסבהנמצתץ"},
 {"ar","العربية",U"ضصثقفغعهخحجدشسيبلاتنمكطئءؤرلاىةوزظأإآّ",U"ضصثقفغعهخحجدشسيبلاتنمكطئءؤرلاىةوزظأإآّ"},
 {"ru","Русский",U"йцукенгшщзхъфывапролджэячсмитьбюё",U"ЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮЁ"},
 {"th","ไทย",U"ๆไำพะัีรนยบลฟหกดเ้่าสวงผปแอิืทมใฝ",U"๐๑๒๓๔๕๖๗๘๙ฎฑธํ๊ณฯญฐฤฆฏโฌ็๋ษศซฉฮฺ์ฒฬฦ"},
 {"ja","日本語 · かな",U"あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわをんー",U"ぁぃぅぇぉがぎぐげござじずぜぞだぢづでどっばびぶべぼぱぴぷぺぽゃゅょゎアイウエオカキクケコ"},
 {"ko","한국어",U"ㅂㅈㄷㄱㅅㅛㅕㅑㅐㅔㅁㄴㅇㄹㅎㅗㅓㅏㅣㅋㅌㅊㅍㅠㅜㅡ",U"ㅃㅉㄸㄲㅆㅛㅕㅑㅒㅖㅁㄴㅇㄹㅎㅗㅓㅏㅣㅋㅌㅊㅍㅠㅜㅡ"},
 // Hanzi/kanji conversion uses the console's IME, not an incomplete dictionary.
 {"zh-Hans","中文 · PS5",U"qwertyuiopasdfghjklzxcvbnm",U"QWERTYUIOPASDFGHJKLZXCVBNM"}
};
inline std::size_t language_index(std::string_view code) {
 for(std::size_t n=0;n<std::size(languages);++n)if(code==languages[n].code)return n;
 return 0;
}
inline bool valid_language(std::string_view code){return code==languages[language_index(code)].code;}
}
