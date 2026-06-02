/* SD card and FAT filesystem example.
   This example uses SPI peripheral to communicate with SD card.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "thermostat.hpp"

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "logger.h"
#include "esp_partition.h"
#include "esp_core_dump.h"

// #include "driver/sdspi_host.h"
// #include "driver/spi_common.h"


#define MOUNT_POINT "/sdcard"
#define EXAMPLE_MAX_CHAR_SIZE 64

const char mount_point[] = MOUNT_POINT;
static const char *TAG = "sdcard";
static sdmmc_card_t *sdcard;

static esp_err_t sd_write_file(const char *path, char *data)
{
  ESP_LOGD(TAG, "Opening file %s", path);
  FILE *f = fopen(path, "w");
  if (f == NULL)
  {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return ESP_FAIL;
  }
  fprintf(f, data);
  fclose(f);
  ESP_LOGD(TAG, "File written");

  return ESP_OK;
}

static esp_err_t sd_read_file(const char *path)
{
  ESP_LOGD(TAG, "Reading file %s", path);
  FILE *f = fopen(path, "r");
  if (f == NULL)
  {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return ESP_FAIL;
  }
  char line[EXAMPLE_MAX_CHAR_SIZE];
  fgets(line, sizeof(line), f);
  fclose(f);

  // strip newline
  char *pos = strchr(line, '\n');
  if (pos)
  {
    *pos = '\0';
  }
  ESP_LOGD(TAG, "Read from file: '%s'", line);

  return ESP_OK;
}

// from: https://github.com/espressif/esp-idf/blob/master/examples/storage/sd_card/sdspi/main/sd_card_example_main.c

void test_sd_card()
{
  // Use POSIX and C standard library functions to work with files.

  // First create a file.
  const char *file_hello = MOUNT_POINT "/hello.txt";
  char data[EXAMPLE_MAX_CHAR_SIZE];
  snprintf(data, EXAMPLE_MAX_CHAR_SIZE, "%s %s!\n", "Hello", sdcard->cid.name);
  esp_err_t ret = sd_write_file(file_hello, data);
  if (ret != ESP_OK)
  {
    return;
  }

  const char *file_foo = MOUNT_POINT "/foo.txt";

  // Check if destination file exists before renaming
  struct stat st;
  if (stat(file_foo, &st) == 0)
  {
    // Delete it if it exists
    unlink(file_foo);
  }

  // Rename original file
  ESP_LOGD(TAG, "Renaming file %s to %s", file_hello, file_foo);
  if (rename(file_hello, file_foo) != 0)
  {
    ESP_LOGE(TAG, "SD Card file rename failed");
    return;
  }

  ret = sd_read_file(file_foo);
  if (ret != ESP_OK)
  {
    return;
  }

  // Format FATFS
#ifdef CONFIG_EXAMPLE_FORMAT_SD_CARD
  ret = esp_vfs_fat_sdcard_format(mount_point, card);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to format FATFS (%s)", esp_err_to_name(ret));
    return;
  }

  if (stat(file_foo, &st) == 0)
  {
    ESP_LOGD(TAG, "file still exists");
    return;
  }
  else
  {
    ESP_LOGW(TAG, "file doesn't exist, formatting done");
  }
#endif // CONFIG_EXAMPLE_FORMAT_SD_CARD

  const char *file_nihao = MOUNT_POINT "/nihao.txt";
  memset(data, 0, EXAMPLE_MAX_CHAR_SIZE);
  snprintf(data, EXAMPLE_MAX_CHAR_SIZE, "%s %s!\n", "Nihao", sdcard->cid.name);
  ret = sd_write_file(file_nihao, data);
  if (ret != ESP_OK)
  {
    return;
  }

  // Open file for reading
  ret = sd_read_file(file_nihao);
  if (ret != ESP_OK)
  {
    return;
  }
}

void gpio_cfg()
{
  gpio_config_t conf;
  conf.pin_bit_mask = 1ULL << SD_CS;
  conf.mode = GPIO_MODE_INPUT;
  conf.pull_up_en = GPIO_PULLUP_ENABLE;
  conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  conf.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&conf);
}

esp_err_t mount_sd_card()
{
  // esp_log_level_set("sdmmc_cmd", ESP_LOG_INFO);

  sdspi_device_config_t device_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  device_config.host_id = SPI3_HOST;
  device_config.gpio_cs = (gpio_num_t)SD_CS;

  ESP_LOGD(TAG, "Initializing SD card");
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = device_config.host_id;

  esp_vfs_fat_mount_config_t mount_config =
  {
    .format_if_mount_failed = true,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024,
    .disk_status_check_enable = true
  };

  ESP_LOGI(TAG, "Mounting filesystem");
  esp_err_t ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &device_config, &mount_config, &sdcard);
  if (ret != ESP_OK)
  {
    if (ret == ESP_FAIL)
    {
      ESP_LOGE(TAG, "Failed to mount filesystem. "
                    "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
      return ESP_FAIL;
    }
    else
    {
      ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                    "Make sure SD card lines have pull-up resistors in place.",
               esp_err_to_name(ret));
      return ESP_FAIL;
    }
    return ESP_FAIL;
  }
  ESP_LOGD(TAG, "Filesystem mounted");

  // Card has been initialized, print its properties
  sdmmc_card_print_info(stdout, sdcard);

  return ESP_OK;
}

esp_err_t unmount_sd_card()
{
  esp_err_t ret = esp_vfs_fat_sdcard_unmount(mount_point, sdcard);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to unmount filesystem: %s ", esp_err_to_name(ret));
    return ret;
  }
  ESP_LOGD(TAG, "Card unmounted");
  return ret;
}

esp_err_t saveConfig()
{
  const char *file_config = MOUNT_POINT "/conf.txt";

  // Mount SDCARD
  mount_sd_card();

  // Check if destination file exists before writing
  struct stat st;
  if (stat(file_config, &st) == 0)
  {
    // Delete it if it exists
    unlink(file_config);
  }

  // Write config and wifi info
  ESP_LOGD(TAG, "Writing file %s", file_config);
  FILE *f = fopen(file_config, "w");
  if (f == NULL)
  {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return ESP_FAIL;
  }
  fwrite((void *)(&OperatingParameters), sizeof(OPERATING_PARAMETERS), 1, f);
  fclose(f);
  ESP_LOGD(TAG, "File written");

  // unmount SDCARD
  unmount_sd_card();

  return ESP_OK;
}

esp_err_t loadConfig()
{
  const char *file_config = MOUNT_POINT "/conf.bin";

  // Mount SDCARD
  mount_sd_card();

  // Check if destination file exists before reading
  struct stat st;
  if (stat(file_config, &st) != 0)
  {
    ESP_LOGE(TAG, "File \"%s\" does not exist", file_config);
    return ESP_FAIL;
  }

  // Read config and wifi info
  ESP_LOGI(TAG, "Loading file %s", file_config);
  FILE *f = fopen(file_config, "r");
  if (f == NULL)
  {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return ESP_FAIL;
  }
  fread((void *)(&OperatingParameters), sizeof(OPERATING_PARAMETERS), 1, f);
  fclose(f);
  ESP_LOGD(TAG, "File loaded");

  // unmount SDCARD
  unmount_sd_card();

  return ESP_OK;
}

int sdcard_esp32_vprintf(const char *fmt, va_list va)
{
  static char sdcardBuffer[256];
  int res = vsnprintf(sdcardBuffer, sizeof(sdcardBuffer), fmt, va);
  // int res = vsprintf(sdcardBuffer, fmt, va);

  const char *file_log = MOUNT_POINT "/log.txt";

  // Mount SDCARD
  if (res = mount_sd_card() != ESP_OK)
  {
    printf ("Failed to mount SD card: %s", esp_err_to_name(res));
    return res;
  }

  // Check if destination file exists before writing
  struct stat st;
  if (stat(file_log, &st) != 0)
  {
    // File doesn't exist, create it
    FILE *f = fopen(file_log, "w");
    if (f == NULL)
    {
      ESP_LOGE(TAG, "Failed to open log file for writing");
      return ESP_FAIL;
    }
    fclose(f);
  }

  // Append log message to file
  FILE *f = fopen(file_log, "a");
  if (f == NULL)
  {
    ESP_LOGE(TAG, "Failed to open log file for appending");
    return ESP_FAIL;
  }
  fprintf(f, "%s", sdcardBuffer);
  fflush(f);
  fclose(f);

  // unmount SDCARD
  unmount_sd_card();

  return res;
}

esp_err_t sd_init(void)
{
  esp_err_t ret = ESP_OK;

  gpio_cfg();

  if (ret = mount_sd_card() != ESP_OK)
  {
    printf ("Failed to mount SD card: %s\n", esp_err_to_name(ret));
    enableSdcardLogging(false);
    return ret;
  }
  unmount_sd_card();
  enableSdcardLogging(true);
  return ret;
}

void sdcard_check_and_save_coredump(const char* sd_file_path) 
{
  size_t coredump_addr = 0;
  size_t coredump_size = 0;

  // 1. Locate and size the core dump currently resting in Flash
  esp_err_t err = esp_core_dump_image_get(&coredump_addr, &coredump_size);
  
  if (err == ESP_ERR_NOT_FOUND)
  {
    ESP_LOGI(TAG, "No core dump found in flash memory.");
    return;
  }
  else if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to inspect core dump partition (%s)", esp_err_to_name(err));
    return;
  }

  ESP_LOGI(TAG, "Found core dump! Size: %d bytes. Copying to SD...", coredump_size);

  if (mount_sd_card() != ESP_OK)
  {
    ESP_LOGE (TAG, "Failed to mount SD card");
    return;
  }

  // 2. Open destination file on your mounted SD card
  FILE *f = fopen(sd_file_path, "wb");
  if (f == NULL)
  {
    ESP_LOGE(TAG, "Failed to open file on SD card for writing");
    unmount_sd_card();
    return;
  }

  // 3. Find the coredump partition explicitly to map and read flash data
  const esp_partition_t *part = esp_partition_find_first(
    ESP_PARTITION_TYPE_DATA, 
    ESP_PARTITION_SUBTYPE_DATA_COREDUMP, 
    NULL
  );

  if (!part)
  {
    ESP_LOGE(TAG, "Coredump partition entry not found!");
    fclose(f);
    unmount_sd_card();
    return;
  }

  // 4. Read data out in small chunks to protect RAM availability 
  uint8_t buffer[512];
  size_t bytes_remaining = coredump_size;
  size_t current_offset = 0;

  while (bytes_remaining > 0)
  {
    size_t to_read = (bytes_remaining > sizeof(buffer)) ? sizeof(buffer) : bytes_remaining;
      
    err = esp_partition_read(part, current_offset, buffer, to_read);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Flash read failed at offset %d", current_offset);
        break;
    }

    size_t written = fwrite(buffer, 1, to_read, f);
    if (written != to_read)
    {
      ESP_LOGE(TAG, "SD Card write error encountered.");
      break;
    }

    bytes_remaining -= to_read;
    current_offset += to_read;
  }

  fclose(f);
  unmount_sd_card();

  if (bytes_remaining == 0)
  {
    ESP_LOGI(TAG, "Core dump successfully transferred to %s", sd_file_path);
      
    // 5. CRITICAL: Clear the flash partition so you don't keep rewriting it every boot
    err = esp_core_dump_image_erase();
    if (err == ESP_OK)
    {
      ESP_LOGI(TAG, "Flash core dump cleared successfully.");
    }
    else
    {
      ESP_LOGE(TAG, "Failed to erase flash core dump (%s)", esp_err_to_name(err));
    }
  }
  else
  {
    ESP_LOGE(TAG, "Failed to transfer entire core dump. %d bytes remaining.", bytes_remaining);
  }
}
