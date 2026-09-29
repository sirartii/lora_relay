#include "../../src/GatewayReceptionBank.h"
#include <cassert>
#include <iostream>
using Bank = opp_lora_relay_official::GatewayReceptionBank;
using R = Bank::Result;

int main()
{
    {
        Bank b;
        for (int c = 1; c <= 8; ++c) b.arrive(c, c, -90, true, false, 0, 10);
        for (int c = 1; c <= 8; ++c) assert(b.complete(c) == R::Delivered);
        assert(b.size() == 0);
    }
    {
        Bank b;
        b.arrive(1, 1, -90, true, false, 0, 10);
        b.arrive(2, 1, -90, true, false, 1, 11);
        assert(b.complete(1) == R::Collision && b.complete(2) == R::Collision);
    }
    for (bool strongerFirst : {true, false}) {
        Bank b;
        b.arrive(1, 1, strongerFirst ? -70 : -90, true, false, 0, 10);
        b.arrive(2, 1, strongerFirst ? -90 : -70, true, false, 1, 11);
        assert(b.complete(1) == (strongerFirst ? R::Delivered : R::Collision));
        assert(b.complete(2) == (strongerFirst ? R::Collision : R::Delivered));
    }
    {
        Bank b;
        b.arrive(1, 1, -90, true, false, 0, 10);
        b.arrive(2, 1, -84, true, false, 1, 11); // exactly 6 dB is not capture
        b.arrive(3, 1, -84, true, false, 2, 12); // failed frames still interfere
        for (int id = 1; id <= 3; ++id) assert(b.complete(id) == R::Collision);
    }
    {
        Bank b;
        b.arrive(1, 1, -90, true, false, 0, 10);
        b.arrive(2, 1, -90, true, false, 10, 20);
        assert(b.complete(1) == R::Delivered && b.complete(2) == R::Delivered);
    }
    {
        Bank b;
        b.arrive(1, 1, -90, true, false, 0, 10);
        b.arrive(2, 8, -90, true, false, 0, 10);
        b.startTransmission(1);
        b.arrive(3, 4, -90, true, true, 2, 12);
        assert(!b.decoding(2));
        b.arrive(4, 4, -90, true, false, 5, 15); // TX ended, missed signal has not
        assert(b.complete(1) == R::TxInterrupted);
        assert(b.complete(2) == R::TxInterrupted);
        assert(b.complete(3) == R::TxBlocked);
        assert(b.complete(4) == R::Collision);
        b.arrive(5, 4, -90, true, false, 15, 25);
        assert(b.complete(5) == R::Delivered);
    }
    {
        Bank b;
        b.arrive(1, 1, -130, false, false, 0, 10);
        b.arrive(2, 1, -80, true, false, 1, 11);
        assert(b.complete(1) == R::Sensitivity);
        assert(b.complete(2) == R::Delivered);
    }
    {
        Bank b;
        bool rejected = false;
        try { b.arrive(1, 10, -90, true, false, 0, 10); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    std::cout << "Gateway reception bank: all assertions passed\n";
}
