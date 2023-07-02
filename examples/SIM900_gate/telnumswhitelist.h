/**
 * @file telnumwhitelist.h
 *
 * @brief Whitelist of tel. numbers allowed to open gates
 * @date 2023-06-23
 * @author F3lda
 * @update 2023-06-23
 */

#ifndef TEL_NUMS_WHITELIST_H
#define TEL_NUMS_WHITELIST_H

#define ADMIN_TEL_NUMBER "+"

// TelNumber Whitelist
//https://www.arduino.cc/reference/en/language/variables/utilities/progmem/
const char whiteList1 [] PROGMEM = "+";
const char whiteList2 [] PROGMEM = "+";
const char whiteList3 [] PROGMEM = "+";
const char whiteList4 [] PROGMEM = "+";
const char whiteList5 [] PROGMEM = "+";
const char whiteList6 [] PROGMEM = "+";
const char whiteList7 [] PROGMEM = "+";
const char whiteList8 [] PROGMEM = "+";
const char whiteList9 [] PROGMEM = "+";
const char whiteList10 [] PROGMEM = "+";
const char whiteList11 [] PROGMEM = "+";
const char whiteList12 [] PROGMEM = "+";
const char whiteList13 [] PROGMEM = "+";

const char * const whitelist[] PROGMEM = {whiteList1, whiteList2, whiteList3, whiteList4, whiteList5, whiteList6, whiteList7, whiteList8, whiteList9, whiteList10, whiteList11, whiteList12, whiteList13};

#endif
