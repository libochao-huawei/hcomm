#ifndef TRACYLITE_PERFETTO_HPP
#define TRACYLITE_PERFETTO_HPP

#ifdef TRACYLITE_PERFETTO

#include <cstdint>
#include <functional>
#include <vector>

namespace tracylite {

class Collector;

class PerfettoNativeExporter {
public:
    enum class CounterTrackMode : uint8_t
    {
        PerProcess = 0,
        PerThread = 1
    };

    // Called with the full serialized trace payload.
    // Return false to signal write failure.
    using WriteCallback = std::function<bool( const uint8_t* data, size_t size )>;

    static void SetCounterTrackMode( CounterTrackMode mode );
    static CounterTrackMode GetCounterTrackMode();

    static bool ExportToFile( const Collector& collector, const char* filename );
    static std::vector<uint8_t> ExportToBuffer( const Collector& collector );

    // Serialize and hand the raw bytes to |writer| (e.g. a log-library sink).
    // Returns false when there is nothing to export or |writer| returns false.
    static bool ExportWithWriter( const Collector& collector, const WriteCallback& writer );
};

} // namespace tracylite

#endif // TRACYLITE_PERFETTO
#endif // TRACYLITE_PERFETTO_HPP
