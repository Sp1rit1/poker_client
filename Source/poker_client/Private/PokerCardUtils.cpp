#include "PokerCardUtils.h"
// Не нужно включать PokerDataTypes.cpp, если реализация ToString() там.
// Но если реализация ToString() была в PokerDataTypes.h (inline), то все ОК.
// Если реализация ToString() в PokerDataTypes.cpp, то этот .cpp должен ее видеть.

FString UPokerCardUtils::Conv_CardToString(const FCard& Card)
{
    return Card.ToRussianString(); // Просто вызываем существующий метод структуры
}