#include "poker_client_tests.h" 

#define LOCTEXT_NAMESPACE "FPokerClientTestsModule"

void FPokerClientTestsModule::StartupModule()
{
    UE_LOG(LogTemp, Warning, TEXT("PokerClientTests module has started"));
}

void FPokerClientTestsModule::ShutdownModule()
{
    UE_LOG(LogTemp, Warning, TEXT("PokerClientTests module has shut down"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPokerClientTestsModule, poker_client_tests)