#ifdef SR_STANDALONE_TEST
#include "catch_amalgamated.hpp"
#else
#include <catch2/catch_test_macros.hpp>
#endif
#include <sightreading/EventQueue.hpp>
#include <thread>
using namespace sightreading;

TEST_CASE("Attack pressure cannot consume release event reserve") {
    EventQueue<32,8> queue;
    for(int i=0;i<24;++i) REQUIRE(queue.push({EventType::On,Bus::Live,1,0,60,100,static_cast<uint64_t>(i)}));
    REQUIRE_FALSE(queue.push({EventType::On}));
    for(int i=0;i<8;++i) REQUIRE(queue.push({EventType::Off}));
    REQUIRE_FALSE(queue.push({EventType::Off}));
    InputEvent event;
    for(int i=0;i<24;++i) { REQUIRE(queue.pop(event)); REQUIRE(event.type==EventType::On); REQUIRE(event.instance==static_cast<uint64_t>(i)); }
    for(int i=0;i<8;++i) { REQUIRE(queue.pop(event)); REQUIRE(event.type==EventType::Off); }
    REQUIRE_FALSE(queue.pop(event));
    REQUIRE(queue.push({EventType::On}));
}

TEST_CASE("Concurrent input producers retain every release without corrupting events") {
    EventQueue<64,16> queue;
    std::array<std::thread,3> producers;
    for(unsigned source=0;source<producers.size();++source) {
        producers[source]=std::thread([&,source] {
            for(uint64_t n=0;n<1000;++n) {
                InputEvent event{EventType::Off,Bus::Live,static_cast<uint16_t>(source),0,60,0,n};
                while(!queue.push(event)) std::this_thread::yield();
            }
        });
    }
    std::array<uint64_t,3> expected{};
    unsigned received=0;
    bool valid=true;
    while(received<3000) {
        InputEvent event;
        if(!queue.pop(event)) { std::this_thread::yield(); continue; }
        if(event.source>=3) valid=false;
        else if(event.instance!=expected[event.source]++) valid=false;
        ++received;
    }
    for(auto& thread:producers) thread.join();
    REQUIRE(valid);
    REQUIRE(expected==std::array<uint64_t,3>{1000,1000,1000});
}
