#include <iostream>
#include <map>
#include <string>
#include <functional>
#include <filesystem>
#include <vector>
#include <fstream>
#include <set>
#include <random>
#include <type_traits>

namespace fs = std::filesystem;
using namespace std;

template <class Server, class Data, class Hash = hash<string> >
class HashRing {
public:
    typedef map<size_t, Server> ServerMap;
    HashRing(unsigned int replicas)
        : replicas_(replicas), hash_(Hash()) {
    }
    HashRing(unsigned int replicas, const Hash& hash)
        : replicas_(replicas), hash_(hash) {
    }
    size_t AddServer(const Server& server);
    void RemoveServer(const Server& server);
    const Server& GetServer(const Data& data) const;
    map<Server, vector<Data>> GetDistribution(const vector<Data>& data) const;

private:
    string StringifyServer(const Server& server) const {
        if constexpr (is_same_v<Server, string>) {
            return server;
        } else {
            return to_string(server);
        }
    }
    string StringifyReplica(unsigned int num) const {
        return to_string(num);
    }
    string StringifyData(const Data& data) const {
        if constexpr (is_same_v<Data, string>) {
            return data;
        } else {
            return to_string(data);
        }
    }
    ServerMap ring_;
    const unsigned int replicas_;
    Hash hash_;
};

template <class Server, class Data, class Hash>
size_t HashRing<Server, Data, Hash>::AddServer(const Server& server) {
    size_t hash;
    string serverstr = StringifyServer(server);

    for (unsigned int r = 0; r < replicas_; r++) {
        // Mix the server string with the replica number to get a unique position
        string replica_string = serverstr + StringifyReplica(r);

        // Compute the hash
        hash = hash_(replica_string.c_str());

        // Add the hash to the ring with the corresponding server
        ring_[hash] = server;
    }
    return hash;
}

template <class Server, class Data, class Hash>
void HashRing<Server, Data, Hash>::RemoveServer(const Server& server) {
    string serverstr = StringifyServer(server);

    for (unsigned int r = 0; r < replicas_; r++) {
        // Mix the server string with the replica number (same as in AddServer)
        string replica_string = serverstr + StringifyReplica(r);

        // Compute the hash
        size_t hash = hash_(replica_string.c_str());

        // Remove the hash from the ring
        ring_.erase(hash);
    }
}

template <class Server, class Data, class Hash>
const Server& HashRing<Server, Data, Hash>::GetServer(const Data& data) const {
    if (ring_.empty()) {
        throw runtime_error("Empty ring");
    }

    size_t hash = hash_(StringifyData(data).c_str());

    // Find the first server with a hash >= data hash
    typename ServerMap::const_iterator it = ring_.lower_bound(hash);

    // If no such server exists, wrap around to the first server in the ring
    if (it == ring_.end()) {
        it = ring_.begin();
    }

    return it->second;
}

template <class Server, class Data, class Hash>
map<Server, vector<Data>> HashRing<Server, Data, Hash>::GetDistribution(const vector<Data>& data) const {
    map<Server, vector<Data>> distribution;
    for (const auto& item : data) {
        Server server = GetServer(item);
        distribution[server].push_back(item);
    }
    return distribution;
}

vector<string> getFilePaths(const string& directory_path, const string& exclude_path) {
    vector<string> file_paths;
    for (const auto& entry : fs::recursive_directory_iterator(directory_path)) {
        if (entry.is_regular_file()) {
            if (entry.path().string().find(exclude_path) == string::npos) {
                file_paths.push_back(entry.path().string());
            }
        }
    }
    return file_paths;
}

void writeDetailedData(const string& filename, int V_NODE, int X,
                       const map<string, int>& before, const map<string, int>& after) {
    ofstream file(filename, ios::app);
    if (file.tellp() == 0) {  // If file is empty, write header
        file << "V_NODE,X,Server,BeforeCount,AfterCount,GainedData\n";
    }

    for (const auto& [server, count_before] : before) {
        int count_after = after.count(server) ? after.at(server) : 0;
        string gained_data = (count_after > count_before) ? "Yes" : "No";
        file << V_NODE << "," << X << "," << server << "," << count_before << "," << count_after << "," << gained_data << "\n";
    }

    file.close();
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <num_servers> <directory_path>" << endl;
        return 1;
    }

    int R_NODE = 1024;
    string directory_path = argv[2];
    string exclude_path = directory_path + "/test";  // Path to exclude
    vector<int> V_NODE_values = {10, 50, 100, 500, 1000};
    vector<int> X_values = {1}; // Number of failed servers
    vector<string> file_paths = getFilePaths(directory_path, exclude_path);

    random_device rd;
    mt19937 gen(rd());

    for (int V_NODE : V_NODE_values) {
        HashRing<string, string> hashRing(V_NODE);
        vector<string> servers;
        for (int i = 1; i <= R_NODE; ++i) {
            string server = "Server" + to_string(i);
            servers.push_back(server);
            hashRing.AddServer(server);
        }

        // Calculate original distribution before failure
        map<string, int> original_distribution;
        map<string, vector<string>> server_to_data;
        for (const string& file : file_paths) {
            string server = hashRing.GetServer(file);
            original_distribution[server]++;
            server_to_data[server].push_back(file);
        }

        for (int X : X_values) {
            // Randomly select a server to fail
            uniform_int_distribution<> dis(0, servers.size() - 1);
            int failed_server_index = dis(gen);
            string failed_server = servers[failed_server_index];
            hashRing.RemoveServer(failed_server);

            // Redistribute only the data from the failed server
            map<string, int> new_distribution = original_distribution;
            new_distribution[failed_server] = 0;  // The failed server now has zero data

            for (const auto& file : server_to_data[failed_server]) {
                string new_server = hashRing.GetServer(file);
                new_distribution[new_server]++;
            }

            // Write detailed data with gained data flag
            writeDetailedData("detailed_data.csv", V_NODE, X, original_distribution, new_distribution);
        }
    }

    return 0;
}

