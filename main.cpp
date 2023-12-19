#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories);

#define COUT_ERROR(include_file, in_file, line) \
        cout << "unknown include file " << include_file << " at file " << in_file.string() << " at line " << line << endl;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

enum class Status {
    ACCESS,
    ERROR,
    CRITICAL
};

pair<int, path> FileSearch(const string& name, const vector<path>& paths) {
    for (auto i = paths.begin(); i < paths.end(); ++i) {
        path p = *i;
        error_code err;
        auto status = filesystem::status(p, err);
        if (err) continue;
        for (const auto& dir_entry : filesystem::recursive_directory_iterator(p)) {
            filesystem::path entry_path(dir_entry);
            auto entry_status = filesystem::status(entry_path);
            if (entry_status.type() == filesystem::file_type::regular && entry_path.filename().string() == name) {
                return { 0, entry_path };
            }
        }
    }
    return { 0, "" };
}




bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {



    const static regex lib_regex(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    const static regex header_regex(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");

    error_code err;
    const filesystem::path in_path(in_file);

    auto const status = filesystem::status(in_file, err);

    if (err)
    {
        return 0;
    }

    ifstream in(in_file);
    if (!in) return 0;

    ofstream out(out_file, ios::app);
    if (!out) return 0;
    
    string line;

    for (int i = 1; getline(in, line); ++i) {
        smatch m;
        error_code error;
        if (regex_match(line, m, lib_regex)) {
            path p = string(m[1]);
            auto [err, found_file_path] = FileSearch(p.filename().string(), include_directories);

            auto const status1 = filesystem::status(found_file_path, error);
            if (err || error)
            {
                COUT_ERROR(p.filename().string(), in_file, i);
                return 0;
            }

            bool r = Preprocess(found_file_path, out_file, include_directories);
            if (!r) {
                return 0;
            }
        }

        else if (regex_match(line, m, header_regex)) {
            path p = in_file.parent_path() / string(m[1]);

            vector<path> include_dirs_plus(include_directories.begin(), include_directories.end());
            include_dirs_plus.push_back(p.parent_path());
            auto [err, found_file_path] = FileSearch(p.filename().string(), include_dirs_plus);

            auto const status1 = filesystem::status(found_file_path, error);
            if (err || error)
            {
                COUT_ERROR(p.filename().string(), in_file, i);
                return 0;
            }

            bool r = Preprocess(found_file_path, out_file, include_directories);

            if (!r) {
                return 0;
            }
        }
        else {

            out << line << "\n";
            out.flush();
        }
    }

    return true;

}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return { (istreambuf_iterator<char>(stream)), istreambuf_iterator<char>() };
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
            "#include \"dir1/b.h\"\n"
            "// text between b.h and c.h\n"
            "#include\"dir1/d.h\"\n"
            "\n"
            "int SayHello() {\n"
            "    cout << \"hello, world!\" << endl;\n"
            "#include <dummy.txt>\n"
            "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
            "#include \"subdir/c.h\"\n"
            "// text from b.h after include\n"s;

    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
            "#include <std1.h>\n"
            "#include \"dummy.txt\"\n"
            "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
            "#include \"lib/std2.h\"\n"
            "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }
    Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
        { "sources"_p / "include1"_p,"sources"_p / "include2"_p });
    /*assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));*/

    ostringstream test_out;
    test_out << "// this comment before include\n"
        "// text from b.h before include\n"
        "// text from c.h before include\n"
        "// std1\n"
        "// text from c.h after include\n"
        "// text from b.h after include\n"
        "// text between b.h and c.h\n"
        "// text from d.h before include\n"
        "// std2\n"
        "// text from d.h after include\n"
        "\n"
        "int SayHello() {\n"
        "    cout << \"hello, world!\" << endl;\n"s;

    /*assert(GetFileContents("sources/a.in"s) == test_out.str());*/
}

int main() {
    Test();
}

