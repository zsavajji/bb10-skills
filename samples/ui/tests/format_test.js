// node app/tests/format_test.js — checks app/assets/format.js, the pages' pure helpers.
var fs = require("fs"), path = require("path"), vm = require("vm"), assert = require("assert");
var src = fs.readFileSync(path.join(__dirname, "..", "assets", "format.js"), "utf8");
var F = {};
// ".pragma library" is QML's, not JavaScript: node would not parse the line.
vm.runInNewContext(src.replace(/^\.pragma library/m, ""), F);
function same(a, b) {   // across the vm's context, deepStrictEqual trips on prototypes
    assert.strictEqual(JSON.stringify(a), JSON.stringify(b));
}

// age
assert.strictEqual(F.age(1000, 1000), "now");
assert.strictEqual(F.age(1000, 1059), "now");
assert.strictEqual(F.age(1000, 1060), "1m");
assert.strictEqual(F.age(1000, 1000 + 3599), "59m");
assert.strictEqual(F.age(1000, 1000 + 3600), "1h");
assert.strictEqual(F.age(1000, 1000 + 86399), "23h");
assert.strictEqual(F.age(1000, 1000 + 2 * 86400), "2d");
assert.strictEqual(F.age(1000, 1000 + 29 * 86400), "29d");
assert.strictEqual(F.age(1000, 1000 + 30 * 86400), "1mo");
assert.strictEqual(F.age(1000, 1000 + 124 * 86400), "4mo");
assert.strictEqual(F.age(1000, 1000 + 364 * 86400), "11mo");
assert.strictEqual(F.age(1000, 1000 + 365 * 86400), "1y");
assert.strictEqual(F.age(1000, 1000 + 800 * 86400), "2y");
assert.strictEqual(F.age(2000, 1000), "now");               // a clock behind the post's

// count
assert.strictEqual(F.count(0), "0");
assert.strictEqual(F.count(999), "999");
assert.strictEqual(F.count(1000), "1k");
assert.strictEqual(F.count(1234), "1.2k");
assert.strictEqual(F.count(9999), "9.9k");
assert.strictEqual(F.count(10000), "10k");
assert.strictEqual(F.count(34567), "34k");
assert.strictEqual(F.count(999999), "999k");
assert.strictEqual(F.count(1000000), "1M");
assert.strictEqual(F.count(1234567), "1.2M");
assert.strictEqual(F.count(34000000), "34M");
assert.strictEqual(F.count(-1234), "-1.2k");
assert.strictEqual(F.count(-5), "-5");
assert.strictEqual(F.count(12.9), "12");
assert.strictEqual(F.count(undefined), "0");

// duration
assert.strictEqual(F.duration(14), "0:14");
assert.strictEqual(F.duration(14.7), "0:14");
assert.strictEqual(F.duration(62), "1:02");
assert.strictEqual(F.duration(3723), "1:02:03");
assert.strictEqual(F.duration(0), "0:00");
assert.strictEqual(F.duration(-3), "0:00");

// splitSort, listingPath
same(F.splitSort("/best"), { base: "", sort: "best" });
same(F.splitSort("/hot"), { base: "", sort: "hot" });
same(F.splitSort("/r/pics/top"), { base: "/r/pics", sort: "top" });
same(F.splitSort("/r/popular/hot"), { base: "/r/popular", sort: "hot" });
same(F.splitSort("/user/x/submitted"), { base: "/user/x/submitted", sort: "" });
same(F.splitSort("/r/pics/search"), { base: "/r/pics/search", sort: "" });
same(F.splitSort("/search"), { base: "/search", sort: "" });
same(F.splitSort("/subreddits/popular"), { base: "/subreddits/popular", sort: "" });
same(F.splitSort("/user/new/submitted"), { base: "/user/new/submitted", sort: "" });   // a user called "new"
same(F.splitSort("/r/new"), { base: "/r/new", sort: "" });                              // a sub called "new"
assert.strictEqual(F.listingPath("/r/pics", "top"), "/r/pics/top");
assert.strictEqual(F.listingPath("", "best"), "/best");

// parseSort, joinSort
same(F.parseSort("top:week", "hot"), { sort: "top", time: "week" });
same(F.parseSort("top", "hot"), { sort: "top", time: "day" });
same(F.parseSort("new:week", "hot"), { sort: "new", time: "" });
same(F.parseSort("", "hot"), { sort: "hot", time: "" });
same(F.parseSort("bogus", "best"), { sort: "best", time: "" });
same(F.parseSort("controversial:zzz", "hot"), { sort: "controversial", time: "day" });
assert.strictEqual(F.joinSort("top", "week"), "top:week");
assert.strictEqual(F.joinSort("new", ""), "new");

// stripHtml
assert.strictEqual(F.stripHtml("<html><p>a &amp; b</p><p>c<br/>d</p></html>"), "a & b\n\nc\nd");
assert.strictEqual(F.stripHtml("<a href=\"https://x\">x &lt;y&gt;</a>"), "x <y>");
assert.strictEqual(F.stripHtml(""), "");

// spoilers
var veiled = "<html>a " + F.SPOILER + "b</span> c</html>";
assert.strictEqual(F.hasSpoiler(veiled), true);
assert.strictEqual(F.hasSpoiler("<html>a</html>"), false);
assert.strictEqual(F.reveal(veiled), "<html>a <span>b</span> c</html>");
assert.strictEqual(F.hasSpoiler(F.reveal(veiled)), false);
assert.strictEqual(F.hasSpoiler(undefined), false);

// the rest
assert.strictEqual(F.bare("t3_1abcde"), "1abcde");
assert.strictEqual(F.bare("1abcde"), "1abcde");
assert.strictEqual(F.bare(undefined), "");
assert.strictEqual(F.truncate("abcdef", 4), "abc…");
assert.strictEqual(F.truncate("abc", 4), "abc");
assert.strictEqual(F.percent(1, 3), 33);
assert.strictEqual(F.percent(5, 0), 0);
assert.strictEqual(F.indent(3, 2, 8), 6);
assert.strictEqual(F.indent(20, 2, 8), 16);
assert.strictEqual(F.permalinkUrl("/r/a/comments/1/b/"), "https://www.reddit.com/r/a/comments/1/b/");
assert.strictEqual(F.levelColor(0), F.levelColor(8));

// the account's writes
assert.strictEqual(F.nextVote(0, 1), 1);
assert.strictEqual(F.nextVote(1, 1), 0);
assert.strictEqual(F.nextVote(-1, 1), 1);
assert.strictEqual(F.nextVote(1, -1), -1);
assert.strictEqual(F.nextVote(-1, -1), 0);
assert.strictEqual(F.voteScore(10, 0, 1), 11);
assert.strictEqual(F.voteScore(10, 1, 0), 9);
assert.strictEqual(F.voteScore(10, -1, 1), 12);
assert.strictEqual(F.voteScore(10, 1, -1), 8);
assert.strictEqual(F.firstLines("a\n\n  b  \nc\nd", 2, 100), "a\nb");
assert.strictEqual(F.firstLines("abcdefgh", 3, 5), "abcd…");
assert.strictEqual(F.firstLines(undefined, 3, 5), "");
assert.strictEqual(F.isWebUrl("https://a.b/c"), true);
assert.strictEqual(F.isWebUrl(" http://a "), true);
assert.strictEqual(F.isWebUrl("ftp://a"), false);
assert.strictEqual(F.isWebUrl("https://"), false);
assert.strictEqual(F.isWebUrl("https://a b"), false);
assert.strictEqual(F.isPictureUrl("https://i.redd.it/x.jpg"), true);
assert.strictEqual(F.isPictureUrl("https://i.redd.it/x.PNG?w=1"), true);
assert.strictEqual(F.isPictureUrl("https://example.com/page"), false);
assert.strictEqual(F.isPictureUrl("https://example.com/a.jpg.html"), false);
assert.strictEqual(F.isPictureUrl(""), false);
assert.strictEqual(F.subName("r/pics"), "pics");
assert.strictEqual(F.subName("/r/pics"), "pics");
assert.strictEqual(F.subName(" pics "), "pics");
assert.strictEqual(F.subName("a"), "");
assert.strictEqual(F.subName("two words"), "");
assert.strictEqual(F.dateText(1700000000), "14 Nov 2023");

assert.deepStrictEqual(F.rebase([{ depth: 5 }, { depth: 6 }, { depth: 5 }], 2), [{ depth: 2 }, { depth: 3 }, { depth: 2 }]);
assert.deepStrictEqual(F.rebase([{ depth: 1 }, { depth: 2 }], 1), [{ depth: 1 }, { depth: 2 }]);
assert.deepStrictEqual(F.rebase([], 4), []);

assert.strictEqual(F.megabytes(1258291), "1.2 MB");
assert.strictEqual(F.megabytes(24117248), "23 MB");
assert.strictEqual(F.progressText(1258291, 24117248), "1.2 MB / 23 MB");
assert.strictEqual(F.progressText(1258291, 0), "1.2 MB");
assert.strictEqual(F.progressText(0, 0), "");
assert.strictEqual(F.ringFrame(0, 0), "00");
assert.strictEqual(F.ringFrame(1, 24), "01");
assert.strictEqual(F.ringFrame(50, 100), "12");
assert.strictEqual(F.ringFrame(100, 100), "24");
assert.strictEqual(F.ringFrame(300, 100), "24");

console.log("format_test: ok");
