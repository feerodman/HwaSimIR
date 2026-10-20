#include "../HwaSim_IR/HwaSim_IR/IR/IRSimulationClock.h"
#include "../HwaSim_IR/HwaSim_IR/IR/IRMaterialThermalState.h"
#include "../HwaSim_IR/HwaSim_IR/IR/IRPhysicalTrace.h"
#include <cassert>
#include <thread>
#include <vector>
#include <iostream>
#include <limits>

int main()
{
    IRSimulationClock clock;
    assert(clock.accept(1, 86399990, "UtcMillisecondsOfDay", "2026-10-21").valid);
    assert(clock.accept(2, 0, "UtcMillisecondsOfDay", "2026-10-21").valid);
    assert(std::abs(clock.state().dtSec-.01)<1e-9);
    assert(clock.accept(3, 0, "UtcMillisecondsOfDay", "2026-10-21").dtSec==0);
    assert(!clock.accept(3, 10, "UtcMillisecondsOfDay", "2026-10-21").valid);
    assert(clock.accept(4, 10, "UtcMillisecondsOfDay", "2026-10-21").valid);
    assert(!clock.accept(5, 5, "UtcMillisecondsOfDay", "2026-10-21").valid);
    assert(clock.accept(6, 20, "UtcMillisecondsOfDay", "2026-10-21").valid);
    assert(std::abs(clock.state().relativeSec-.03)<1e-9);
    assert(!clock.accept(7, 7200020, "UtcMillisecondsOfDay", "2026-10-21").valid);
    assert(clock.accept(8, 30, "UtcMillisecondsOfDay", "2026-10-21").valid);
    clock.reset();
    assert(clock.accept(1, 0, "UnixEpochMs", "").valid);
    assert(clock.state().relativeSec==0);
    assert(clock.accept(2, 30000, "UnixEpochMs", "").dtSec==30);

    IRMaterialThermalModel model;
    IRMaterialThermalOptions options; options.enabled=true;
    IRMaterialThermalProperties properties;
    IRMaterialThermalInput input;
    input.directIrradianceWm2=800; input.diffuseIrradianceWm2=100;
    IRMaterialThermalState once, split;
    input.dtSec=10;
    assert(model.update(input, properties, options, &once).valid);
    input.dtSec=.1;
    for(int i=0;i<100;++i) assert(model.update(input, properties, options, &split).valid);
    for(int i=0;i<6;++i) assert(std::abs(once.solarDeltaK[i]-split.solarDeltaK[i])<1e-12);
    input.dtSec=0;
    auto held=model.update(input, properties, options, &once);
    assert(held.valid);
    assert(held.solarDeltaK==split.solarDeltaK);
    input.dtSec=3601;
    assert(!model.update(input, properties, options, &once).valid);
    input.dtSec=.1; input.directIrradianceWm2=std::numeric_limits<double>::quiet_NaN();
    assert(!model.update(input, properties, options, &once).valid);

    assert(IRPhysicalTrace::instance().enabled());
    std::vector<std::thread> threads;
    for(int t=0;t<4;++t) threads.emplace_back([t]{
        for(int i=0;i<500;++i){
            IRPhysicalTrace::Record r;
            r.sourceSeq=t*500+i+1; r.generation=1;
            r.timeValid=true; r.utcMs=1792540800000.0+i*10; r.relativeSec=i*.01;
            r.targetKey="34/3/3"; r.band="SWIR"; r.datasetId="UNIT_TEST_ONLY";
            r.cellId="TEST"; r.cameraStatus="MATCHED_REFERENCE";
            r.overall="PARTIAL"; r.error="test_missing_other_components";
            r.input["rangeKm"]=.0001;
            r.components["tau"]={.99,"1","VALID"};
            IRPhysicalTrace::instance().write(r);
        }
    });
    for(auto& thread:threads) thread.join();
    IRPhysicalTrace::instance().flush();
    std::cout<<"PASS clock ordering/rollover/dt; thermal subdivision/hold; 2000 threaded records\n";
}
