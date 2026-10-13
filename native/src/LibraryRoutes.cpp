#include <chrono>
#include <cmath>
#include <fstream>
#include <sightreading/LibraryRoutes.hpp>
#include <sightreading/Transport.hpp>
namespace sightreading {
using json = nlohmann::json;
static void output(httplib::Response &res, const json &j) {
  res.set_content(j.dump(), "application/json");
  res.set_header("Cache-Control", "no-store");
}
static void error(httplib::Response &res, const std::exception &e,
                  int status = 400) {
  res.status = status;
  output(res, {{"error",
                {{"code", status == 404 ? "song_not_found" : "invalid_song"},
                 {"message", e.what()}}}});
}
void libraryRoutes(httplib::Server &http, Storage &storage, unsigned rate, FingeringJobs &jobs) {
  auto fingeringError = [](httplib::Response &res, const std::exception &e) {
    auto specific=dynamic_cast<const FingeringError*>(&e);
    int status=specific?specific->status:400;
    if(!specific && std::string(e.what()).find("Song not found")!=std::string::npos)status=404;
    res.status=status;output(res,{{"error",{{"code",specific?specific->code:(status==404?"song_not_found":"invalid_fingering")},{"message",e.what()}}}});
  };
  http.Post(R"(/api/v1/songs/([^/]+)/fingering/generate)",
            [&storage, &jobs, fingeringError](const auto &req, auto &res) {
              try {
                res.status = 202;
                output(res, jobs.start(req.matches[1]));
              } catch (const std::exception &e) {
                fingeringError(res, e);
              }
            });
  http.Get(R"(/api/v1/songs/([^/]+)/fingering)",
           [&storage, &jobs, fingeringError](const auto &req, auto &res) {
             try {
               auto song = storage.song(req.matches[1]);
               output(
                   res,
                   {{"job", jobs.snapshot()},
                    {"revision", song.value("fingeringRevision", uint64_t(0))},
                    {"summary", song.value("fingeringRun", json(nullptr))}});
             } catch (const std::exception &e) {
               fingeringError(res, e);
             }
           });
  http.Post(R"(/api/v1/songs/([^/]+)/fingering/cancel)",
            [&storage, &jobs, fingeringError](const auto &req, auto &res) {
              try {
                storage.song(req.matches[1]);
                auto body = json::parse(req.body);
                auto job = jobs.snapshot();
                if (job["songId"] != std::string(req.matches[1]))
                  throw FingeringError(409, "fingering_stale",
                                       "Job belongs to another song");
                jobs.cancel(body.at("jobId").template get<std::string>());
                output(res, jobs.snapshot());
              } catch (const std::exception &e) {
                fingeringError(res, e);
              }
            });
  http.Patch(R"(/api/v1/songs/([^/]+)/fingering)", [&storage, fingeringError](
                                                       const auto &req,
                                                       auto &res) {
    try {
      auto body = json::parse(req.body);
      if (!body.is_object() || !body.contains("noteIds") ||
          !body["noteIds"].is_array() || body["noteIds"].empty() ||
          !body.contains("finger"))
        throw std::invalid_argument(
            "Select numeric note IDs and a finger or null");
      std::vector<uint64_t> ids;
      for (auto &id : body["noteIds"]) {
        if (!id.is_number_integer() || id.template get<int64_t>() < 0)
          throw std::invalid_argument("Note IDs must be nonnegative integers");
        ids.push_back(id.template get<uint64_t>());
      }
      std::optional<int> finger;
      if (!body["finger"].is_null()) {
        if (!body["finger"].is_number_integer() ||
            body["finger"].template get<int64_t>() < 1 ||
            body["finger"].template get<int64_t>() > 5)
          throw std::invalid_argument("Finger must be 1 to 5 or null");
        finger = body["finger"].template get<int>();
      }
      auto song = storage.editFingering(req.matches[1], ids, finger);
      output(res, {{"revision", song.value("fingeringRevision", uint64_t(0))},
                   {"summary", song.value("fingeringRun", json(nullptr))}});
    } catch (const std::exception &e) {
      fingeringError(res, e);
    }
  });
  struct WindowCache {
    std::mutex mutex;
    std::string id;
    uint64_t revision{};
    std::unique_ptr<SongWindow> window;
  };
  // One current song bounds cache memory; storage revision invalidates every
  // edit.
  auto cache = std::make_shared<WindowCache>();
  http.Get("/api/v1/songs",
           [&](auto &, auto &res) { output(res, storage.songs()); });
  http.Get("/api/v1/songs/trash",
           [&](auto &, auto &res) { output(res, storage.trashSongs()); });
  http.Post("/api/v1/songs/import", [&](const auto &req, auto &res) {
    try {
      if (!req.is_multipart_form_data() || !req.form.has_file("file"))
        throw std::invalid_argument("Choose a .mid or .midi file.");
      auto f = req.form.get_file("file");
      auto extension = std::filesystem::path(f.filename).extension().string();
      std::transform(extension.begin(), extension.end(), extension.begin(),
                     ::tolower);
      if (extension != ".mid" && extension != ".midi")
        throw std::invalid_argument("Expected .mid or .midi.");
      auto song = importMidi(f.content,
                             std::filesystem::path(f.filename).stem().string());
      song["id"] =
          "import-" +
          std::to_string(
              std::chrono::steady_clock::now().time_since_epoch().count());
      song["recent"] =
          uint64_t(std::chrono::system_clock::now().time_since_epoch().count());
      storage.putSong(song, f.content);
      song.erase("rawEvents");
      res.status = 201;
      output(res, song);
    } catch (const std::exception &e) {
      error(res, e);
    }
  });
  http.Get(R"(/api/v1/songs/([^/]+))", [&](const auto &req, auto &res) {
    try {
      auto song = storage.song(req.matches[1]);
      song.erase("notes");
      song.erase("controllers");
      song.erase("rawEvents");
      output(res, song);
    } catch (const std::exception &e) {
      error(res, e, 404);
    }
  });
  http.Get(
      R"(/api/v1/songs/([^/]+)/original)", [&](const auto &req, auto &res) {
        try {
          res.set_content(storage.originalSong(req.matches[1]), "audio/midi");
        } catch (const std::exception &e) {
          error(res, e, 404);
        }
      });
  http.Get(R"(/api/v1/songs/([^/]+)/events)", [&storage, rate, cache](const auto &req,
                                                               auto &res) {
    try {
      std::lock_guard lock(cache->mutex);
      const std::string id = req.matches[1];
      const auto revision = storage.libraryRevision();
      if (!cache->window || cache->id != id || cache->revision != revision) {
        cache->window = std::make_unique<SongWindow>(storage.song(id));
        cache->id = id; cache->revision = revision;
      }
      double from = req.has_param("fromTick")
                        ? std::stod(req.get_param_value("fromTick"))
                        : 0,
             to = req.has_param("toTick")
                      ? std::stod(req.get_param_value("toTick"))
                      : cache->window->ppq() * 16;
      unsigned limit = req.has_param("limit")
                           ? std::stoul(req.get_param_value("limit"))
                           : 4096;
      if (!std::isfinite(from) || !std::isfinite(to) || from < 0 || to < from ||
          !limit || limit > 4096)
        throw std::invalid_argument("Invalid event window; limit 1–4096.");
      auto notes = cache->window->query(from, to, rate, limit + 1);
      const bool truncated = notes.size() > limit;
      if (truncated) notes.erase(notes.end()-1);
      output(res, {{"notes", notes}, {"truncated", truncated},
                   {"measures", cache->window->measures(from, to, rate)},
                   {"inspected", cache->window->inspected()},
                   {"fromTick", from},
                   {"toTick", to},
                   {"limit", limit}});
    } catch (const std::exception &e) {
      error(res, e);
    }
  });
  http.Patch(R"(/api/v1/songs/([^/]+))", [&](const auto &req, auto &res) {
    try {
      storage.patchSong(req.matches[1], json::parse(req.body));
      output(res, storage.song(req.matches[1]));
    } catch (const std::exception &e) {
      error(res, e);
    }
  });
  http.Delete(R"(/api/v1/songs/([^/]+))", [&](const auto &req, auto &res) {
    try {
      storage.trashSong(req.matches[1], true);
      output(res, {{"trashed", true}});
    } catch (const std::exception &e) {
      error(res, e, 404);
    }
  });
  http.Post(R"(/api/v1/songs/([^/]+)/restore)",
            [&](const auto &req, auto &res) {
              try {
                storage.trashSong(req.matches[1], false);
                output(res, storage.song(req.matches[1]));
              } catch (const std::exception &e) {
                error(res, e, 404);
              }
            });
}
void installStarters(Storage &storage, const std::filesystem::path &folder) {
  for (const auto &name : {"manifest.json", "tutorials.json"}) {
  std::ifstream manifest(folder / name);
  if (!manifest)
    continue;
  auto list = json::parse(manifest);
  for (auto &entry : list) {
    std::string id = entry["id"];
    try {
      storage.song(id);
      continue;
    } catch (const std::invalid_argument &) {
    }
    std::ifstream f(folder / entry["file"].get<std::string>(),
                    std::ios::binary);
    if (!f) {
      // Optional tutorial downloads are provisioned by setup-tutorials.
      if (entry.contains("tutorial")) continue;
      throw std::runtime_error("Starter MIDI missing.");
    }
    std::string bytes((std::istreambuf_iterator<char>(f)), {});
    auto song = importMidi(bytes, entry["title"]);
    song["id"] = id;
    song["chords"] = entry["chords"];
    song["provenance"] = entry["provenance"];
    if (entry.contains("tutorial")) song["tutorial"] = entry["tutorial"];
    song["recent"] = uint64_t(0);
    storage.putSong(song, bytes);
  }
  }
}
} // namespace sightreading
