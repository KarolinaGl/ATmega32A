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

int16_t firstNumber = 0; // Globalna zmienna przechowuj�ca pierwszy sk�adnik dzia�ania
int16_t secondNumber = 0; // Globalna zmienna przechowuj�ca drugi sk�adnik dzia�ania
int16_t result = 0; // Globalna zmienna przechowuj�ca wynik dzia�ania
uint8_t numberOfDigits = 0; // Globalna zmienna przechowuj�ca ilo�� cyfr wpisywanej liczby

// Funkcja przyjmuj�ca numer wiersza i zwracaj�ca wci�ni�t� kolumn� wiersza
int8_t getKey3(uint8_t row){
// Wyprowadzenia PD0-PD3 odpowiadaj� wierszom 0-3 klawiatury
	if (row == 0) // Dla wiersza numer 0
		PORTD = 0b00000001; // Ustawienie pull-up na wyprowadzeniu PD0
	else if (row == 1) // Dla wiersza numer 1
		PORTD = 0b00000010; // Ustawienie pull-up na wyprowadzeniu PD1
	else if (row == 2) // Dla wiersza numer 2
		PORTD = 0b00000100; // Ustawienie pull-up na wyprowadzeniu PD2
	else if (row == 3) // Dla wiersza numer 3
		PORTD = 0b00001000; // Ustawienie pull-up na wyprowadzeniu PD3
// Wyprowadzenia PD6-PD4 odpowiadaj� kolumnom 0-2 klawiatury
	if (bit_is_set(PIND, PD6)) // Je�eli wci�ni�to przycisk z kolumny PD6
		return 0; // Zwr�� 0
	else if (bit_is_set(PIND, PD5)) // Je�eli wci�ni�to przycisk z kolumny PD5
		return 1; // Zwr�� 1
	else if (bit_is_set(PIND, PD4)) // Je�eli wci�ni�to przycisk z kolumny PD4
		return 2; // Zwr�� 2
	return -1; // W przypadku braku wci�ni�tego przycisku zwr�� -1
}

// Funkcja zwracaj�ca numer wci�ni�tego przycisku
int8_t getKey12(){
	int8_t key = -1; // Domy�lny numer przycisku w przypadku niewci�ni�cia
	for (uint8_t row = 0; row < 4; ++row){ // P�tla po wszystkich wierszach klawiatury
		int8_t column = getKey3(row); // Pobranie wci�ni�tej kolumny
		if (column != -1) // Je�eli wci�ni�to jakikolwiek przycisk
			key = (column + 1) + row * 3; // Obliczenie numeru przycisku
	}
	if (key == 11) // Je�li wci�ni�to przycisk jedenasty (na klawiaturze �0�)
		return 0; // Zwr�� 0
	else if (key == 12) // Je�li wci�ni�to przycisk dwunasty (na klawiaturze �#�)
		return 11; // Zwr�� 11
	else if (bit_is_clear(PINC, PC0)) //Je�li wci�ni�to dodatkowy przycisk po prawej
		return 12; // Zwr�� 12
	return key; // Zwr�� numer przycisku
}

// Funkcja tworz�ca liczb� z wci�ni�tych na klawiaturze cyfr
void createSecondNumber(uint8_t digit){
	if (digit != 10 && digit != 11 && digit != 12) // Je�eli wci�ni�to przycisk inny ni� "*", "#", "="
	{
		secondNumber *= 10; // Obecnie przechowywana liczba wzrasta o rz�d wielko�ci
		secondNumber += digit; // Do przechowywanej liczby dodawana jest wci�ni�ta cyfra
		numberOfDigits++; // Zwi�kszanie ilo�ci cyfr przechowywanej liczby
	}
}

// Funkcja wy�wietlaj�ca liczb� na wy�wietlaczu LCD przyjmuj�ca liczb� i pozycj�, na kt�rej zostanie wy�wietlona
void displayNumber(int16_t* number, uint8_t position){
	char text[20]; // Tablica znak�w do przechowywania wy�wietlanego tekstu
	LCD_GoTo(0, position); // Ustawienie wsp�rz�dnych ekranowych
	if (numberOfDigits <= 4) // Je�eli liczba ma mniej ni� lub r�wno 4 cyfry
		sprintf(text, "liczba: %d", *number); // Wype�nienie tablicy znak�w
	else { // Je�eli liczba ma wi�cej ni� 4 cyfry
		sprintf(text, "poza zakresem"); // Wype�nienie tablicy znak�w
		*number = 0; // Wyzerowanie liczby spoza zakresu
	}
	LCD_WriteText(text); // Wy�wietlenie tekstu na wy�wietlaczu
}

int main() {
	DDRD = 0x00; // Ustawienie ca�ego portu D jako wej�cia
	sbi(PORTC, PC0); // Ustawienie wyprowadzenia PC0 na stan wysoki
	LCD_Initalize(); // Inicjalizacja wy�wietlacza LCD
	LCD_Home(); // Przywr�cenie pocz�tkowych wsp�rz�dnych wy�wietlacza
	char text[20]; // Ci�g znak�w, do kt�rego wpisany zostanie komunikat
	uint8_t addition = 0; // Zmienna pomocnicza okre�laj�ca, czy wykonywane jest dodawanie czy odejmowanie
	uint8_t isSolved = 0; // Zmienna pomocnicza okre�laj�ca, czy obliczono wynik dzia�ania

	while (1) { // Niesko�czona p�tla
		LCD_Clear(); // Czyszczenie ekranu wy�wietlacza
		if (getKey12() != -1){ // Je�eli wci�ni�to jakikolwiek przycisk
			if (getKey12() == 10) { // Je�eli wci�ni�to przycisk "*" odpowiadaj�cy dodawaniu
				firstNumber = secondNumber; // Zamiana warto�ci dw�ch liczb
				secondNumber = 0; // Wyzerowanie drugiej liczby
				addition = 1; // Zaznaczenie, �e wykonywane jest dodawanie
				numberOfDigits = 0; // Wyzerowanie ilo�ci cyfr obecnej liczby
			}
			else if (getKey12() == 11){ // Je�eli wci�ni�to przycisk "#" odpowiadaj�cy odejmowaniu
				firstNumber = secondNumber; // Zamiana warto�ci dw�ch liczb
				secondNumber = 0; // Wyzerowanie drugiej liczby
				addition = 0; // Zaznaczenie, �e wykonywane jest odejmowanie
				numberOfDigits = 0; // Wyzerowanie ilo�ci cyfr obecnej liczby
			}
			if (getKey12() == 12 && addition) // Je�eli wci�ni�to "=" i wykonywane jest dodawanie
			{
				result = firstNumber + secondNumber; // Do wyniku wpisz sum� liczb
				isSolved = 1; // Zaznaczenie, �e wynik zosta� wyliczony
			}
			else if (getKey12() == 12 && !addition){// Je�eli wci�ni�to "=" i wykonywane jest odejmowanie
				result = firstNumber - secondNumber; // Do wyniku wpisz r�nic� liczb
				isSolved = 1; // Zaznaczenie, �e wynik zosta� wyliczony
			}
			createSecondNumber(getKey12()); // Wczytywanie kolejnej liczby
		}
		if (isSolved) { // Je�eli wynik zosta� wyliczony
			LCD_GoTo(0, 1); // Ustawienie wsp�rz�dnych ekranowych
			if (result >= 0 && result <= 9999){ // Je�eli wynik mie�ci si� w zakresie <0, 9999>
				if (addition) // Je�eli wykonane zosta�o dodawanie
					sprintf(text, "suma: %d", result); // Wype�nienie tablicy znak�w
				else // Je�eli wykonane zosta�o odejmowanie
					sprintf(text, "roznica: %d", result); // Wype�nienie tablicy znak�w
			}
			else // Je�eli wynik nie mie�ci si� w zakresie <0, 9999>
				sprintf(text, "poza zakresem"); // Wype�nienie tablicy znak�w
			LCD_WriteText(text); // Wy�wietlenie tekstu na wy�wietlaczu
			_delay_ms(100); // Op�nienie o 100 ms
			if (getKey12() == 12){ // Je�eli wci�ni�to znak "="
				LCD_Clear(); // Czyszczenie ekranu wy�wietlacza
				firstNumber = 0; // Wyzerowanie pierwszej liczby
				secondNumber = 0; // Wyzerowanie drugiej liczby
				result = 0; // Wyzerowanie wyniku
				numberOfDigits = 0; // Wyzerowanie ilo�ci cyfr obecnej liczby
				isSolved = 0; // Zaznaczenie, �e nast�pny wynik nie zosta� wyliczony
			}
		}
		else { // Je�eli wynik nie zosta� wyliczony
			displayNumber(&firstNumber, 1); // Wy�wietl pierwsz� liczb�
			displayNumber(&secondNumber, 0); // Wy�wietl drug� liczb�
		}
		_delay_ms(200); // Op�nienie o 200 ms
	}
}
