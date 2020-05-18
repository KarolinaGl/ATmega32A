#include <avr/io.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <util/delay.h>
#include <avr/sfr_defs.h>
#include <math.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include "HD44780.h"

#ifndef _BV
#define _BV(bit)				(1<<(bit))
#endif
#ifndef sbi
#define sbi(reg,bit)		reg |= (_BV(bit))
#endif

#ifndef cbi
#define cbi(reg,bit)		reg &= ~(_BV(bit))
#endif

int16_t firstNumber = 0; // Globalna zmienna przechowuj¹ca pierwszy sk³adnik dzia³ania
int16_t secondNumber = 0; // Globalna zmienna przechowuj¹ca drugi sk³adnik dzia³ania
int16_t result = 0; // Globalna zmienna przechowuj¹ca wynik dzia³ania
uint8_t numberOfDigits = 0; // Globalna zmienna przechowuj¹ca iloœæ cyfr wpisywanej liczby

// Funkcja przyjmuj¹ca numer wiersza i zwracaj¹ca wciœniêt¹ kolumnê wiersza
int8_t getKey3(uint8_t row){
// Wyprowadzenia PD0-PD3 odpowiadaj¹ wierszom 0-3 klawiatury
	if (row == 0) // Dla wiersza numer 0
		PORTD = 0b00000001; // Ustawienie pull-up na wyprowadzeniu PD0
	else if (row == 1) // Dla wiersza numer 1
		PORTD = 0b00000010; // Ustawienie pull-up na wyprowadzeniu PD1
	else if (row == 2) // Dla wiersza numer 2
		PORTD = 0b00000100; // Ustawienie pull-up na wyprowadzeniu PD2
	else if (row == 3) // Dla wiersza numer 3
		PORTD = 0b00001000; // Ustawienie pull-up na wyprowadzeniu PD3
// Wyprowadzenia PD6-PD4 odpowiadaj¹ kolumnom 0-2 klawiatury
	if (bit_is_set(PIND, PD6)) // Je¿eli wciœniêto przycisk z kolumny PD6
		return 0; // Zwróæ 0
	else if (bit_is_set(PIND, PD5)) // Je¿eli wciœniêto przycisk z kolumny PD5
		return 1; // Zwróæ 1
	else if (bit_is_set(PIND, PD4)) // Je¿eli wciœniêto przycisk z kolumny PD4
		return 2; // Zwróæ 2
	return -1; // W przypadku braku wciœniêtego przycisku zwróæ -1
}

// Funkcja zwracaj¹ca numer wciœniêtego przycisku
int8_t getKey12(){
	int8_t key = -1; // Domyœlny numer przycisku w przypadku niewciœniêcia
	for (uint8_t row = 0; row < 4; ++row){ // Pêtla po wszystkich wierszach klawiatury
		int8_t column = getKey3(row); // Pobranie wciœniêtej kolumny
		if (column != -1) // Je¿eli wciœniêto jakikolwiek przycisk
			key = (column + 1) + row * 3; // Obliczenie numeru przycisku
	}
	if (key == 11) // Jeœli wciœniêto przycisk jedenasty (na klawiaturze „0”)
		return 0; // Zwróæ 0
	else if (key == 12) // Jeœli wciœniêto przycisk dwunasty (na klawiaturze „#”)
		return 11; // Zwróæ 11
	else if (bit_is_clear(PINC, PC0)) //Jeœli wciœniêto dodatkowy przycisk po prawej
		return 12; // Zwróæ 12
	return key; // Zwróæ numer przycisku
}

// Funkcja tworz¹ca liczbê z wciœniêtych na klawiaturze cyfr
void createSecondNumber(uint8_t digit){
	if (digit != 10 && digit != 11 && digit != 12) // Je¿eli wciœniêto przycisk inny ni¿ "*", "#", "="
	{
		secondNumber *= 10; // Obecnie przechowywana liczba wzrasta o rz¹d wielkoœci
		secondNumber += digit; // Do przechowywanej liczby dodawana jest wciœniêta cyfra
		numberOfDigits++; // Zwiêkszanie iloœci cyfr przechowywanej liczby
	}
}

// Funkcja wyœwietlaj¹ca liczbê na wyœwietlaczu LCD przyjmuj¹ca liczbê i pozycjê, na której zostanie wyœwietlona
void displayNumber(int16_t* number, uint8_t position){
	char text[20]; // Tablica znaków do przechowywania wyœwietlanego tekstu
	LCD_GoTo(0, position); // Ustawienie wspó³rzêdnych ekranowych
	if (numberOfDigits <= 4) // Je¿eli liczba ma mniej ni¿ lub równo 4 cyfry
		sprintf(text, "liczba: %d", *number); // Wype³nienie tablicy znaków
	else { // Je¿eli liczba ma wiêcej ni¿ 4 cyfry
		sprintf(text, "poza zakresem"); // Wype³nienie tablicy znaków
		*number = 0; // Wyzerowanie liczby spoza zakresu
	}
	LCD_WriteText(text); // Wyœwietlenie tekstu na wyœwietlaczu
}

int main() {
	DDRD = 0x00; // Ustawienie ca³ego portu D jako wejœcia
	sbi(PORTC, PC0); // Ustawienie wyprowadzenia PC0 na stan wysoki
	LCD_Initalize(); // Inicjalizacja wyœwietlacza LCD
	LCD_Home(); // Przywrócenie pocz¹tkowych wspó³rzêdnych wyœwietlacza
	char text[20]; // Ci¹g znaków, do którego wpisany zostanie komunikat
	uint8_t addition = 0; // Zmienna pomocnicza okreœlaj¹ca, czy wykonywane jest dodawanie czy odejmowanie
	uint8_t isSolved = 0; // Zmienna pomocnicza okreœlaj¹ca, czy obliczono wynik dzia³ania

	while (1) { // Nieskoñczona pêtla
		LCD_Clear(); // Czyszczenie ekranu wyœwietlacza
		if (getKey12() != -1){ // Je¿eli wciœniêto jakikolwiek przycisk
			if (getKey12() == 10) { // Je¿eli wciœniêto przycisk "*" odpowiadaj¹cy dodawaniu
				firstNumber = secondNumber; // Zamiana wartoœci dwóch liczb
				secondNumber = 0; // Wyzerowanie drugiej liczby
				addition = 1; // Zaznaczenie, ¿e wykonywane jest dodawanie
				numberOfDigits = 0; // Wyzerowanie iloœci cyfr obecnej liczby
			}
			else if (getKey12() == 11){ // Je¿eli wciœniêto przycisk "#" odpowiadaj¹cy odejmowaniu
				firstNumber = secondNumber; // Zamiana wartoœci dwóch liczb
				secondNumber = 0; // Wyzerowanie drugiej liczby
				addition = 0; // Zaznaczenie, ¿e wykonywane jest odejmowanie
				numberOfDigits = 0; // Wyzerowanie iloœci cyfr obecnej liczby
			}
			if (getKey12() == 12 && addition) // Je¿eli wciœniêto "=" i wykonywane jest dodawanie
			{
				result = firstNumber + secondNumber; // Do wyniku wpisz sumê liczb
				isSolved = 1; // Zaznaczenie, ¿e wynik zosta³ wyliczony
			}
			else if (getKey12() == 12 && !addition){// Je¿eli wciœniêto "=" i wykonywane jest odejmowanie
				result = firstNumber - secondNumber; // Do wyniku wpisz ró¿nicê liczb
				isSolved = 1; // Zaznaczenie, ¿e wynik zosta³ wyliczony
			}
			createSecondNumber(getKey12()); // Wczytywanie kolejnej liczby
		}
		if (isSolved) { // Je¿eli wynik zosta³ wyliczony
			LCD_GoTo(0, 1); // Ustawienie wspó³rzêdnych ekranowych
			if (result >= 0 && result <= 9999){ // Je¿eli wynik mieœci siê w zakresie <0, 9999>
				if (addition) // Je¿eli wykonane zosta³o dodawanie
					sprintf(text, "suma: %d", result); // Wype³nienie tablicy znaków
				else // Je¿eli wykonane zosta³o odejmowanie
					sprintf(text, "roznica: %d", result); // Wype³nienie tablicy znaków
			}
			else // Je¿eli wynik nie mieœci siê w zakresie <0, 9999>
				sprintf(text, "poza zakresem"); // Wype³nienie tablicy znaków
			LCD_WriteText(text); // Wyœwietlenie tekstu na wyœwietlaczu
			_delay_ms(100); // OpóŸnienie o 100 ms
			if (getKey12() == 12){ // Je¿eli wciœniêto znak "="
				LCD_Clear(); // Czyszczenie ekranu wyœwietlacza
				firstNumber = 0; // Wyzerowanie pierwszej liczby
				secondNumber = 0; // Wyzerowanie drugiej liczby
				result = 0; // Wyzerowanie wyniku
				numberOfDigits = 0; // Wyzerowanie iloœci cyfr obecnej liczby
				isSolved = 0; // Zaznaczenie, ¿e nastêpny wynik nie zosta³ wyliczony
			}
		}
		else { // Je¿eli wynik nie zosta³ wyliczony
			displayNumber(&firstNumber, 1); // Wyœwietl pierwsz¹ liczbê
			displayNumber(&secondNumber, 0); // Wyœwietl drug¹ liczbê
		}
		_delay_ms(200); // OpóŸnienie o 200 ms
	}
}
