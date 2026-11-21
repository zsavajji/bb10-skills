package main

import (
	"bytes"
	"context"
	"crypto/tls"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strconv"
	"time"
)

// Message translation (docs/plan/translate.md): one function per service, each translating one text.
// The text leaves WhatsApp's encryption for the service the user picked, so nothing here logs a text or a key.

// Variables so the tests point them at httptest servers.
var (
	googleURL    = "https://translate.googleapis.com/translate_a/single"
	googleAPIURL = "https://translation.googleapis.com/language/translate/v2" // with a key: the official API
	deeplFreeURL = "https://api-free.deepl.com"
	deeplProURL  = "https://api.deepl.com"
)

// Google's endpoint answers Go's HTTP/2 client with 429 "Sorry..." at the first request, whatever the User-Agent,
// and the same request over HTTP/1.1 with 200 (seen from the Mac and from Docker, 2026-11-20): so its client has
// HTTP/2 off. A transport of its own, not a clone of the default one, whose TLS config already offers h2 once it
// was used. The other services take the default client.
var googleClient = &http.Client{Transport: &http.Transport{
	TLSNextProto:        map[string]func(string, *tls.Conn) http.RoundTripper{},
	TLSHandshakeTimeout: 10 * time.Second,
	MaxIdleConnsPerHost: 4, // translate_msg.go's requests at once
	IdleConnTimeout:     90 * time.Second,
}}

const (
	transTimeout = 15 * time.Second
	transBodyMax = 1 << 20
	// [...] app logic left out: googleChunk, the size of the pieces a long text is cut into
)

// [...] app logic left out: translateText (which service), transSettings, the cooldown after "too many requests", langBase

// byStatus is a mapper of failures that goes by the HTTP status alone.
func byStatus(m map[int]string) func(int, []byte) string {
	return func(status int, _ []byte) string { return m[status] }
}

// postTrans posts one request and returns the body of a 200. No answer is offline; a status that is not 200 is the
// code the mapper gives for its status and body, else translate_failed with the status. A rate_limited answer starts
// the variant's pause, a 200 clears it.
func postTrans(ctx context.Context, cli *http.Client, variant, u, ctype string, hdr map[string]string, body []byte, codes func(int, []byte) string) ([]byte, error) {
	ctx, cancel := context.WithTimeout(ctx, transTimeout)
	defer cancel()
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, u, bytes.NewReader(body))
	if err != nil {
		return nil, &Error{"translate_failed", "translate: bad service address"}
	}
	req.Header.Set("Content-Type", ctype)
	for k, v := range hdr {
		req.Header.Set(k, v)
	}
	resp, err := cli.Do(req)
	if err != nil {
		return nil, &Error{"offline", "translate: no answer from the service"}
	}
	defer resp.Body.Close()
	b, err := io.ReadAll(io.LimitReader(resp.Body, transBodyMax))
	if err != nil {
		return nil, &Error{"offline", "translate: the answer broke off"}
	}
	if resp.StatusCode != http.StatusOK {
		c := codes(resp.StatusCode, b)
		if c == "" {
			c = "translate_failed"
		}
		if c == "rate_limited" {
			secs, _ := strconv.Atoi(resp.Header.Get("Retry-After")) // an HTTP date is ignored
			transLimited(variant, time.Duration(secs)*time.Second)
		}
		return nil, &Error{c, fmt.Sprintf("translate: HTTP %d", resp.StatusCode)}
	}
	transOK(variant)
	return b, nil
}

var badAnswer = &Error{"translate_failed", "translate: the answer is not what the service sends"}

// [...] app logic left out: chunkText and translateGoogle, which cut a long text and call googleOnce once per piece

func googleOnce(ctx context.Context, text, to string) (out, from string, err error) {
	q := url.Values{"client": {"gtx"}, "sl": {"auto"}, "tl": {to}, "dt": {"t"}}
	b, err := postTrans(ctx, googleClient, "google", googleURL+"?"+q.Encode(), "application/x-www-form-urlencoded",
		map[string]string{"User-Agent": "Mozilla/5.0 (Linux; Android 10) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0 Mobile Safari/537.36"},
		[]byte(url.Values{"q": {text}}.Encode()), byStatus(map[int]string{429: "rate_limited"}))
	if err != nil {
		return "", "", err
	}
	// [[["piece","original",...], ...], null, "en", ...]
	var v []any
	if json.Unmarshal(b, &v) != nil || len(v) < 3 {
		return "", "", badAnswer
	}
	// [...] app logic left out: pulling the pieces and the detected language out of v
	return sb.String(), from, nil
}

// [...] app logic left out: the official API with a key, DeepL and LibreTranslate, one function each
