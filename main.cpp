#include <iostream>
#include <chrono>
#include <vector>
#include <numeric>
#include <math.h>
#include "time_lapse.h"
#include <fstream>
#include "file_loader.h"
#include <boost/filesystem.hpp>
#include <sstream>
extern "C" {
    #include "croaring/roaring.h"
}


using timer = measure<std::chrono::nanoseconds>;
using String = std::string;

template <class T>
using Vec2d = std::vector<std::vector<T>>;

namespace fs = boost::filesystem;

String get_cpu_name(){
    FILE* pipe = popen("cat /proc/cpuinfo | grep 'model name' | sed -n 1p | cut -d ':' -f 2 | awk '{$1=$1};1' | tr -d '\n'", "r");
    char buffer[128];
    String result = "";
    while(!feof(pipe)) {
        if(fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);
    return result;
}

double avg(std::vector<uint16_t > const& v) {
    return 1.0 * std::accumulate(v.begin(), v.end(), 0LL) / v.size();
}

double stdev(std::vector<uint16_t > const& v, const double &avg){
    double accum = 0.0;
    std::for_each (std::begin(v), std::end(v), [&](const double d) {
        accum += (d - avg) * (d - avg);
    });
    auto denum = (v.size()-1) > 0 ? (v.size()-1) : 1;
    return std::sqrt(accum / denum);
}

long range(const uint16_t &min1, const uint16_t &max1, const uint16_t &min2, const uint16_t &max2){
    return std::max(max1, max2) - std::min(min1, min2);
}

void writeToFile(const String &dataset, const String &out_path){
    fs::path p{out_path};
    fs::path dir = p.parent_path();
    fs::create_directories(dir);

    std::ofstream file;
    file.open (out_path);
    file << dataset;
    file.close();
}

template<typename F>
String buildDataset(const Vec2d<uint16_t> &vals, const std::vector<String> &algoNames, F&& func){
    String dataset = "range,n1,average1,std1,n2,average2,std2";
    for(auto a : algoNames) {
        dataset += "," + a;
    }
    dataset += ",fastest_algo";
    dataset += "\n";

    //WriteTofile(append = false)
    std::cout <<  "computing stats..."  << std::endl;
    std::vector<double> avgs{};
    std::vector<double> stdevs{};
    std::vector<uint16_t > mins{};
    std::vector<uint16_t> maxs{};

    for(auto v : vals) {
        auto a = avg(v);
        avgs.push_back(a);
        stdevs.push_back(stdev(v, a));
        mins.push_back(*std::min_element(v.begin(), v.end()));
        maxs.push_back(*std::max_element(v.begin(), v.end()));
    }

    std::cout <<  "running algos.."  << std::endl;
    for(auto i = 0; i < vals.size(); i++){
        for(auto j = 0; j < vals.size(); j++){
            auto rng = range(mins[i], maxs[i], mins[j], maxs[j]);
            std::stringstream ss;
            auto times = fw(func)(vals[i], vals[j]);

            ss << rng << "," << vals[i].size() << "," << avgs[i] << "," << stdevs[i] << ","
                << vals[j].size() << "," << avgs[j] << "," << stdevs[j];
            for(auto t : times) {
                ss << "," << t;
            }
            auto it = std::find(times.begin(), times.end(), *std::min_element(times.begin(), times.end()));
            auto fastestAlgoIndex = std::distance(times.begin(), it);
            ss << "," << algoNames[fastestAlgoIndex];
            ss << "\n";
            dataset += ss.str();
        }
    }
    std::cout <<  "dataset done. \n"  << std::endl;
    return dataset;
}

std::vector<long> run_intersect_algos(const std::vector<uint16_t> &v1, const std::vector<uint16_t> &v2) {
    std::vector<long> times{};
    auto card = std::max(v1.size(), v2.size());
    auto *v3 = new uint16_t[card];

    auto time = timer::execution( intersect_skewed_uint16, v1.data(), v1.size(), v2.data(), v2.size(), v3);
    times.push_back(time);

    time = timer::execution( intersect_skewed_uint16, v2.data(), v2.size(), v1.data(), v1.size(), v3);
    times.push_back(time);

    time = timer::execution( intersect_uint16, v1.data(), v1.size(), v2.data(), v2.size(), v3);
    times.push_back(time);

    delete[] v3;
    return times;
}

std::vector<long> run_intersect_card_algos(const std::vector<uint16_t> &v1, const std::vector<uint16_t> &v2) {
    std::vector<long> times{};

    auto time = timer::execution( intersect_skewed_uint16_cardinality, v1.data(), v1.size(), v2.data(), v2.size());
    times.push_back(time);

    time = timer::execution( intersect_skewed_uint16_cardinality, v2.data(), v2.size(), v1.data(), v1.size());
    times.push_back(time);

    time = timer::execution( intersect_uint16_cardinality, v1.data(), v1.size(), v2.data(), v2.size());
    times.push_back(time);

    time = timer::execution( intersect_vector16_cardinality, v1.data(), v1.size(), v2.data(), v2.size());
    times.push_back(time);

    return times;
}

Vec2d <uint16_t> load_data(const String &data_path){
    size_t *howmany;
    size_t count;
    std::vector<std::vector<uint16_t>> vals{};

    std::cout <<  "Loading data...\n"  << std::endl;
    uint32_t **values = read_all_integer_files(data_path.c_str(), ".txt", &howmany, &count);

    for(auto i = 0; i < count; i++) {
        std::vector<uint16_t> v{};
        for(auto j = 0; j < howmany[i]; j++) {
            if (values[i][j] > std::numeric_limits<uint16_t>::max()) {
                v.push_back((uint16_t) (values[i][j] >> 16));
            }
            v.push_back((uint16_t) (values[i][j] & 0xFFFF));
        }
        vals.push_back(v);
    }
    for(auto i = 0; i < count; i++){
        delete[] values[i];
    }
    delete[] values;
    delete[] howmany;
    return vals;
}

int main() {

    fs::directory_iterator it{fs::system_complete("../realdata")};
    Vec2d <uint16_t> masterData{};
    auto gpu_name = get_cpu_name();
    auto i = 0;
    //Yeah.. that's how boost wants us to iterate in order to support older versions.
    for(; it != fs::directory_iterator(); ++it){
        auto dir = *it;
        auto dataName = dir.path().filename().string();
        std::cout <<  "Opening " << dataName << "..."  << std::endl;
        auto data = load_data(dir.path().string());

        //We will need other sets separated in order to test the masterset
        if(dataName != "test_data")
            masterData.insert(std::end(masterData), std::begin(data), std::end(data));

        fs::path out_path = fs::system_complete("../results/" + gpu_name + "/" + dataName);

        //Intersect_dataset
        auto filename = "/Intersect_dataset.csv";
        std::cout << "building " << filename << std::endl;
        auto dataset = buildDataset(data, {"skewed_1", "skewed_2", "non_skewed"}, run_intersect_algos);
        writeToFile(dataset, out_path.string() +  filename);

        //Intersect_card_dataset
        filename = "/Intersect_card_dataset.csv";
        std::cout << "building " << filename << std::endl;
        dataset = buildDataset(data, {"skewed_1", "skewed_2", "non_skewed", "vector"}, run_intersect_card_algos);
        writeToFile(dataset, out_path.string() + filename);
        i++;
    }

    auto out_path = fs::system_complete("../results/" + gpu_name);

    //Intersect_masterset
    auto filename = "/Intersect_masterset.csv";
    std::cout << "building " << filename << std::endl;
    auto dataset = buildDataset(masterData, {"skewed_1", "skewed_2", "non_skewed"}, run_intersect_algos);
    writeToFile(dataset, out_path.string() +  filename);

    //Intersect_card_masterset
    filename = "/Intersect_card_masterset.csv";
    std::cout << "building " << filename << std::endl;
    dataset = buildDataset(masterData, {"skewed_1", "skewed_2", "non_skewed", "vector"}, run_intersect_card_algos);
    writeToFile(dataset, out_path.string() + filename);


    return 0;
}