"""MkDocs hook: give every page the address of the same page in the other language, when it exists.

The site holds the English and the French pages side by side (en/, fr/). A reader who is in one language should be one click away from the other,
on the very page they are reading, not only from the home page. The rule is computed, never listed by hand, and a counterpart is offered only if
its file exists.
"""
import re

_SPECIAL = {"bestiary.md": "bestiaire.md", "campaigns/": "campagnes/"}


def counterpart(src_uri, existing):
    """The src_uri of the page in the other language, or None. `existing` is the set of src_uris of the site."""
    if src_uri == "index.md":
        cand = "fr/README.md"
    elif src_uri == "fr/README.md":
        cand = "index.md"
    elif src_uri.startswith("en/"):
        rest = src_uri[3:]
        for en, fr in _SPECIAL.items():
            rest = rest.replace(en, fr, 1) if rest.startswith(en) else rest
        cand = "fr/" + rest
    elif src_uri.startswith("fr/"):
        rest = src_uri[3:]
        for en, fr in _SPECIAL.items():
            rest = rest.replace(fr, en, 1) if rest.startswith(fr) else rest
        cand = "en/" + rest
    else:
        return None
    return cand if cand in existing else None


def on_files(files, config):
    config.extra["_src_uris"] = {f.src_uri for f in files.documentation_pages()}
    config.extra["_urls"] = {f.src_uri: f.url for f in files.documentation_pages()}
    return files


def on_page_context(context, page, config, nav):
    other = counterpart(page.file.src_uri, config.extra.get("_src_uris", set()))
    page.meta["other_language_url"] = config.extra["_urls"][other] if other else None
    return context
