# sd_file_server

A component to access the file on an sdcard from a browser

# Config

```yaml
sd_file_server:
  id: file_server
  url_prefix: file
  root_path: "/"
```

* **url_prefix**: (Optional, string, default="file") : url prefix to acces the file page
* **root_path**: (Optional, string, default="/"): sd card file system root