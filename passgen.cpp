#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <cstdlib>
#include <vector>
#include <openssl/hmac.h>
#include <openssl/evp.h>

static std::string trim(const std::string& s) {
    size_t start = 0, end = s.size();
    while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

static std::string capitalize_first(const std::string& s) {
    if (s.empty()) return s;
    std::string out = s;
    out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    return out;
}

static std::string current_local_timestamp_jst() {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    std::tm local_tm;
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&local_tm, &t);
#else
    localtime_r(&t, &local_tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S") << " JST";
    return oss.str();
}

static void print_usage(const char* prog) {
    std::cerr
        << "Usage:\n"
        << "  " << prog << " [wordlist.txt] [--count N] [--words N] [--log <file>] [--no-capitalize]\n\n"
        << "Defaults:\n"
        << "  wordlist.txt     = words.txt\n"
        << "  --count N        = 5\n"
        << "  --words N        = 2\n"
        << "  --log <file>     = generator.log\n"
        << "  capitalization   = enabled (use --no-capitalize to disable)\n";
}

static std::string get_env_or_empty(const char* key) {
    const char* v = std::getenv(key);
    return v ? std::string(v) : std::string{};
}

// Minimal .env loader: KEY=VALUE lines, ignores blanks and comments (#)

static void set_env_if_missing(const std::string& key, const std::string& val) {
    if (std::getenv(key.c_str()) != nullptr) return;

#if defined(_WIN32) || defined(_WIN64)
    _putenv_s(key.c_str(), val.c_str());
#else
    setenv(key.c_str(), val.c_str(), 0); // do not overwrite existing env
#endif
}

static void load_dotenv(const std::string& env_path) {
    std::ifstream in(env_path);
    if (!in) return;

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = trim(line.substr(0, pos));
        std::string val = trim(line.substr(pos + 1));

        // strip optional surrounding quotes
        if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') ||
                                (val.front() == '\'' && val.back() == '\''))) {
            val = val.substr(1, val.size() - 2);
        }

        if (!key.empty()) {
            set_env_if_missing(key, val); // don't overwrite existing env
        }
    }
}

static std::string base32_encode_no_pad(const unsigned char* data, size_t len) {
    static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::string out;
    out.reserve((len * 8 + 4) / 5);

    uint32_t buffer = 0;
    int bits_left = 0;

    for (size_t i = 0; i < len; ++i) {
        buffer = (buffer << 8) | data[i];
        bits_left += 8;
        while (bits_left >= 5) {
            int idx = (buffer >> (bits_left - 5)) & 0x1F;
            bits_left -= 5;
            out.push_back(alphabet[idx]);
        }
    }
    if (bits_left > 0) {
        int idx = (buffer << (5 - bits_left)) & 0x1F;
        out.push_back(alphabet[idx]);
    }
    return out;
}

static std::string pepper_tag4(const std::string& base_password, const std::string& pepper) {
    // HMAC-SHA256(base_password, pepper)
    unsigned int out_len = 0;
    unsigned char digest[EVP_MAX_MD_SIZE];

    HMAC(EVP_sha256(),
         pepper.data(), static_cast<int>(pepper.size()),
         reinterpret_cast<const unsigned char*>(base_password.data()),
         base_password.size(),
         digest, &out_len);

    std::string b32 = base32_encode_no_pad(digest, out_len);
    if (b32.size() < 4) return "AAAA";
    return b32.substr(0, 4);
}

int main(int argc, char* argv[]) {
    // Defaults (as requested)
    std::string wordspath = "words.txt";
    int count = 5;
    int words_per_password = 2;
    bool do_cap = true;
    std::string logfile;
    std::string envfile = ".env";
    bool use_pepper = false;

    load_dotenv(envfile);
    std::string pepper = get_env_or_empty("PASSGEN_PEPPER");

    if (use_pepper && pepper.empty()) {
        std::cerr << "Warning: PASSGEN_PEPPER not set (env file: " << envfile
                << "). Pepper tag will be disabled.\n";
        use_pepper = false;
    }

    // Optional first positional argument: word list path
    int i = 1;
    if (i < argc) {
        std::string first = argv[i];
        if (!first.empty() && first.rfind("--", 0) != 0) {
            wordspath = first;
            ++i;
        }
    }

    // Parse flags
    for (; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }

        if (arg == "--count" && i + 1 < argc) {
            count = std::max(1, std::stoi(argv[++i]));
        } else if (arg == "--words" && i + 1 < argc) {
            words_per_password = std::max(1, std::stoi(argv[++i]));
        } else if (arg == "--log" && i + 1 < argc) {
            logfile = argv[++i];
        } else if (arg == "--capitalize") {
            // Supported for compatibility; capitalization is already the default.
            do_cap = true;
        } else if (arg == "--no-capitalize") {
            do_cap = false;
        } else if (arg == "--wordspath" && i + 1 < argc) {
            // Optional explicit flag (in addition to positional)
            wordspath = argv[++i];
        } else if (arg == "--pepper") {
            use_pepper = true;
        } else if (arg == "--env" && i + 1 < argc) {
            envfile = argv[++i];
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (use_pepper) {
        load_dotenv(envfile); // safe even if file doesn't exist
        std::string pepper = get_env_or_empty("PASSGEN_PEPPER");

        if (pepper.empty()) {
            std::cerr << "Error: --pepper was set but PASSGEN_PEPPER is not defined.\n"
                    << "Set PASSGEN_PEPPER in your environment or create a .env file.\n";
            return 1;
        }

        // store pepper somewhere accessible for generation loop (declare pepper outside if needed)
    }

    // Load words
    std::ifstream in(wordspath);
    if (!in) {
        std::cerr << "Failed to open word list: " << wordspath << "\n";
        return 1;
    }

    std::vector<std::string> words;
    std::string line;
    while (std::getline(in, line)) {
        auto w = trim(line);
        if (!w.empty()) words.push_back(w);
    }

    if (words.size() < static_cast<size_t>(words_per_password)) {
        std::cerr << "Word list must contain at least " << words_per_password << " non-empty lines.\n";
        return 1;
    }

    // Randomness setup
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> fourDigits(0, 9999);

    // Open log file for appending
    std::ofstream logstream;
    if (!logfile.empty()) {
        logstream.open(logfile, std::ios::app);
        if (!logstream) {
            std::cerr << "Error: could not open log file '" << logfile << "'.\n";
            return 1;
        }
    }

    for (int n = 0; n < count; ++n) {
        std::vector<std::string> pick;
        std::sample(words.begin(), words.end(), std::back_inserter(pick),
                    static_cast<size_t>(words_per_password), gen);

        if (pick.size() < static_cast<size_t>(words_per_password)) {
            std::cerr << "Internal error: failed to select " << words_per_password << " words.\n";
            return 1;
        }

        if (do_cap) {
            for (auto& w : pick) w = capitalize_first(w);
        }

        int num = fourDigits(gen);

        std::ostringstream oss;
        for (int w = 0; w < words_per_password; ++w) {
            if (w > 0) oss << "-";
            oss << pick[w];
        }
        oss << "-" << std::setw(4) << std::setfill('0') << num;

        std::string password = oss.str();
        std::string pepper; // empty unless --pepper is used

        if (use_pepper) {
            load_dotenv(envfile);
            pepper = get_env_or_empty("PASSGEN_PEPPER");
            if (pepper.empty()) { return 1; }

            password += "-" + pepper_tag4(password, pepper);
        }

        std::cout << password << "\n";

        if (logstream) {
            logstream << current_local_timestamp_jst() << " - " << password << "\n";
            logstream.flush();
        }
    }

    return 0;
}
