FROM python:3.13-slim

ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    DATA_DIR=/data \
    CONFIG_PATH=/config/config.yaml

WORKDIR /srv
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# rclone reads Google Drive using its own registered Google app (no Cloud project needed).
COPY --from=rclone/rclone:1 /usr/local/bin/rclone /usr/local/bin/rclone

COPY app ./app

RUN useradd --uid 1000 --create-home app \
    && mkdir -p /data /config \
    && chown app:app /data
USER app

# 8080: API and setup page. 53682: used only during the Google Drive sign-in.
EXPOSE 8080 53682
VOLUME ["/data"]
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s \
    CMD python -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8080/healthz', timeout=3)"

CMD ["uvicorn", "app.asgi:app", "--host", "0.0.0.0", "--port", "8080"]
