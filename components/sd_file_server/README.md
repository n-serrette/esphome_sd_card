# sd_file_server

A component to access the file on an storage device from a browser

# Config

This component require a storage device component implementing the [storage_base](components/storage_base/README.md) interface to be configured.

This component allow multiple configurations.

```yaml
sd_file_server:
  id: file_server
  url_prefix: file
  storage_id: storage_1
  root_path: "/"
  enable_deletion: true
  enable_download: true
  enable_upload: true
```

* **url_prefix**: (Optional, string, default="file") : url prefix to acces the file page (ex : sd-card.local/file)
* **storage_id**: (Optional, string): id of the storage device to use
* **root_path**: (Optional, string, default="/"): storage file system root
* **enable_deletion**: (Optional, boolean, default=False): enable file deletion from the web page or api
* **enable_download**: (Optional, boolean, default=False): enable file download from the web page or api
* **enable_upload**: (Optional, boolean, default=False): enable file upload from the web page or api

# Notes

* Trying to download large file will saturate the memory of the esp and make it crash

## esp-idf

Deleting/uploading a file with the esp-idf framework does not seams to work for some reasons.