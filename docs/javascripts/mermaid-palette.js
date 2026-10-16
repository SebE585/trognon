// Match Mermaid's own theme to the site's light/dark toggle instead of
// always rendering Mermaid's default white-background theme, which clashes
// on the slate (dark) palette. Official recipe from mkdocs-material's own
// "Diagrams" docs, re-run on every page load (document$) since this theme
// uses instant navigation for some pages and a plain reload for others.
document$.subscribe(function () {
  var palette = __md_get("__palette");
  var scheme = palette && typeof palette.color === "object" ? palette.color.scheme : "default";
  if (window.mermaid) {
    mermaid.initialize({
      startOnLoad: true,
      theme: scheme === "slate" ? "dark" : "default",
    });
  }
});
