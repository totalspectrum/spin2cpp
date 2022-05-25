/**
 * @file include/locale.h
 * @brief Provides multi-lingual localization API functions.
 *
 * C localization functions are used in multilingual programs to adapt
 * to the specific locale for displaying numbers and currency.
 * Functions affect the C Standard Library I/O function behavior.
 *
 * The only locales supported is "C" and a C.UTF-8 locale which is
 * the same as C but supports UTF-8 multibyte character encoding.
 * Things like currency and so forth are hard coded to the C locale.
 */

/*
 * from the MiNT library (public domain)
 */

#ifndef _LOCALE_H
#define _LOCALE_H

#include <compiler.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Selects category names for the entire locale.
 */
#define LC_ALL          0x001F
/**
 * @brief Affects the behavior of strcoll and strxfrm functions.
 */
#define LC_COLLATE      0x0001
/**
 * @brief Affects the behavior of the character handling and multibyte functions.
 */
#define LC_CTYPE        0x0002
/**
 * @brief Affects the monetary format information returned by localeconv.
 */
#define LC_MONETARY     0x0004
/**
 * @brief Affects the decimal point character and other information for formatted I/O.
 */
#define LC_NUMERIC      0x0008
/**
 * @brief Affects the behavior of the strftime and strfxtime functions.
 */
#define LC_TIME         0x0010

#ifndef NULL
#include <sys/null.h>
#endif

/**
 * @brief lconv contains members related to the formatting of numeric values.
 */
struct lconv {
/**
 * @brief The decimal-point character used to format non-monetary
 * quantities.
 */
        char    *decimal_point;
/**
 * @brief The character used to separate groups of digits in non-monetary
 * quantities.
 */
        char    *thousands_sep;
/**
 * @brief A string that indicates the size of each group of digits in
 * non-monetary quantities.
 */
        char    *grouping;
/**
 * @brief The decimal-point used to format monetary quantities.
 */
        char    *mon_decimal_point;
/**
 * @brief The separator for groups of digits before the decimal-point
 * in monetary quantities.
 */
        char    *mon_thousands_sep;
/**
 * @brief A string that indicates the size of each group of digits in
 * monetary quantities.
 */
        char    *mon_grouping;
/**
 * @brief A string that indicates a non-negative formatted value in
 * monetary quantities.
 */
        char    *positive_sign;
/**
 * @brief A string that indicates a negative formatted value in monetary
 * quantities.
 */
        char    *negative_sign;
/**
 * @brief The currency symbol applicable in the current locale.
 */
        char    *currency_symbol;

/**
 * @brief The number of fractional digits to be displayed in a locally
 * formatted monetary quantity.
 */
        char    frac_digits;
/**
 * @brief Set to 1 or 0 if currency_symbol preceeds or succeeds
 * respectively the non-negative value of monetary quantity.
 */
        char    p_cs_precedes;
/**
 * @brief Set to 1 or 0 if currency_symbol preceeds or succeeds
 * respectively the negative value of monetary quantity.
 */
        char    n_cs_precedes;
/**
 * @brief Set to a value indicating the separation of the currency_symbol,
 * the sign, and the value for non-negative monetary quantity.
 */
        char    p_sep_by_space;
/**
 * @brief Set to a value indicating the separation of the currency_symbol,
 * the sign, and the value for negative monetary quantity.
 */
        char    n_sep_by_space;
/**
 * @brief Set to a value indicating the position of the positive_sign
 * for non-negative monetary quantity.
 */
        char    p_sign_posn;
/**
 * @brief Set to a value indicating the position of the positive_sign
 * for negative monetary quantity.
 */
        char    n_sign_posn;

/**
 * @brief The international currency symbol applicable to the current locale.
 *
 * The first three characters contain the alphabetic international
 * currency symbol in accordance with those specified in ISO 4217:1995.
 * The fourth character (immediately preceding the null character) is
 * the character used to separate the international currency symbol
 * from the monetary quantity.
 */
        char    *int_curr_symbol; 

/**
 * @brief The number of fractional digits to be displayed in a monetary quantity.
 */
        char    int_frac_digits;
/**
 * @brief Set to 1 or 0 if the int_currency_symbol precedes or succeeds
 * respectively the value for a non-negative internationally formatted
 * monetary quantity.
 */
        char    int_p_cs_precedes;
/**
 * @brief Set to 1 or 0 if the int_currency_symbol precedes or succeeds
 * respectively the value for a negative internationally formatted
 * monetary quantity.
 */
        char    int_n_cs_precedes;
/**
 * @brief Set to a value indicating the separation of the int_currency_symbol
 * the sign string, and the value for non-negative international monetary
 * formatted quantities.
 */
        char    int_p_sep_by_space;
/**
 * @brief Set to a value indicating the separation of the int_currency_symbol
 * the sign string, and the value for negative international monetary
 * formatted quantities.
 */
        char    int_n_sep_by_space;
/**
 * @brief Set to a value indicating the position of the positive_sign
 * for non-negative international monetary formatted quantity.
 */
        char    int_p_sign_posn;
/**
 * @brief Set to a value indicating the position of the positive_sign
 * for negative international monetary formatted quantity.
 */
        char    int_n_sign_posn;

};

/**
 * Select the part of the program's locale as specified by the category and locale parameters.
 * The function can be used to change or query the program's current locale.
 *
 * The default is supposed to be setlocale(LC_ALL, "C");
 * 
 * @param category A macro to select the Locale portion to select. @see LC_ALL, etc...
 * @param locale A string specifying the locale environment. Normally "C" for our use.
 * If locale is NULL, setlocale will return the category of the current locale.
 * @returns A pointer to a string associated with the category for the locale, or 0 on error.
 */
char *setlocale(int category, const char *locale);

/**
 * @returns An object of type struct lconv according to the current locale.
 */
struct lconv *localeconv(void);

#ifdef __cplusplus
}
#endif

#endif /* _LOCALE_H */

