#pragma once
#include <gsl/span>
#include <Media/AudioArray.hpp>
#include <QCryptographicHash>
#include <QDir>
#include <QStandardPaths>
#include <QFile>

namespace Media
{

using rms_sample_t = uint16_t;
struct RMSData : public QObject
{
  W_OBJECT(RMSData)
public:
  struct Header
  {
    uint32_t sampleRate{};
    uint32_t bufferSize{};
    uint32_t channels{};
    uint32_t padding{};
  };

  RMSData();

  void load(QString abspath);
  bool exists() const;

  void decode(const std::vector<gsl::span<const ossia::audio_sample>>& audio);
  void decodeLast(const std::vector<gsl::span<const ossia::audio_sample> >& audio);

  rms_sample_t valueAt(int64_t start_sample, int64_t end_sample, int32_t channel) const noexcept;
  ossia::small_vector<rms_sample_t, 8> frame(int64_t start_sample, int64_t end_sample) const noexcept;

  int64_t frames_count = 0;
  int64_t samples_count = 0;

  void newData() W_SIGNAL(newData);
  void finishedDecoding(ossia::audio_handle hdl) W_SIGNAL(finishedDecoding, hdl);
  private:
  QFile m_file;
  bool m_exists{false};


  Header* header{};
  rms_sample_t* data{};

  rms_sample_t computeChannelRMS(gsl::span<const ossia::audio_sample> chan, int64_t start_idx, int64_t buffer_size);
  void computeRMS(const std::vector<gsl::span<const ossia::audio_sample>>& audio, int buffer_size);
  void computeLastRMS(const std::vector<gsl::span<const ossia::audio_sample> >& audio, int buffer_size);

};

}
