"""Tests for tools/check_cta.py: the calls to action that bring content back must not be dead ends."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_cta as cc  # noqa: E402

BASE = cc.BASE


def page(html):
    p = cc.Page()
    p.feed(html)
    return p


STRIP = ('<article><aside class="page-actions"><ul>'
         '<li><a href="%s/issues/new?template=correction.yml&amp;title=x&amp;page=y">c</a></li>'
         '<li><a href="%s/issues/new?template=observation.yml">o</a></li>'
         '<li><a href="%s/edit/main/docs/a.md">e</a></li>'
         '<li><a href="https://x/trognon/en/missions/">m</a></li></ul></aside></article>') % (BASE, BASE, BASE)
FORMS = {"correction.yml": {"body": [{"type": "input", "id": "page", "attributes": {"label": "P"}}]},
         "observation.yml": {"body": [{"type": "textarea", "id": "looked", "attributes": {"label": "L"}}]}}


def problems(html, tracked=("docs/a.md",), generated=()):
    return cc.check_page("p/index.html", page(html), set(tracked), set(generated), FORMS)


class PageTests(unittest.TestCase):
    def test_a_complete_page_has_no_problem(self):
        self.assertEqual(problems(STRIP), [])

    def test_a_page_without_the_strip_is_refused(self):
        self.assertTrue(any("no participation strip" in e for e in problems("<article>text</article>")))

    def test_a_strip_missing_a_link_is_refused(self):
        self.assertTrue(any("no missions link" in e for e in problems(STRIP.replace("en/missions/", "en/other/"))))

    def test_an_edit_link_to_a_missing_file_is_refused(self):
        self.assertTrue(any("not in the repository" in e for e in problems(STRIP, tracked=())))

    def test_an_edit_link_to_a_generated_file_is_refused(self):
        self.assertTrue(any("generated file" in e for e in problems(STRIP, generated=("docs/a.md",))))

    def test_an_issue_link_to_an_unknown_template_is_refused(self):
        self.assertTrue(any("does not exist" in e for e in problems(STRIP.replace("correction.yml", "nope.yml"))))

    def test_an_issue_link_prefilling_an_unknown_field_is_refused(self):
        self.assertTrue(any("does not have" in e for e in problems(STRIP.replace("page=y", "unknownfield=y"))))

    def test_title_and_labels_are_always_allowed(self):
        self.assertEqual(problems(STRIP.replace("page=y", "labels=a")), [])


class FormTests(unittest.TestCase):
    def good(self):
        return {"name": "n", "description": "d", "body": [{"type": "markdown", "attributes": {"value": "v"}},
                                                          {"type": "input", "id": "a", "attributes": {"label": "A"}, "validations": {"required": True}}]}

    def test_a_valid_form(self):
        self.assertEqual(cc.check_form("f", self.good()), [])

    def test_missing_top_level_keys(self):
        self.assertTrue(any("missing 'name'" in e for e in cc.check_form("f", {"description": "d", "body": [1]})))

    def test_an_input_needs_an_id_and_a_label(self):
        f = self.good()
        f["body"][1] = {"type": "input", "attributes": {}}
        errs = cc.check_form("f", f)
        self.assertTrue(any("needs an id" in e for e in errs) and any("needs attributes.label" in e for e in errs))

    def test_duplicate_ids_are_refused(self):
        f = self.good()
        f["body"].append({"type": "textarea", "id": "a", "attributes": {"label": "B"}})
        self.assertTrue(any("duplicate id" in e for e in cc.check_form("f", f)))

    def test_an_unknown_element_type_is_refused(self):
        f = self.good()
        f["body"].append({"type": "slider", "id": "z", "attributes": {"label": "Z"}})
        self.assertTrue(any("unknown element type" in e for e in cc.check_form("f", f)))

    def test_required_must_be_a_boolean(self):
        f = self.good()
        f["body"][1]["validations"] = {"required": "yes"}
        self.assertTrue(any("true or false" in e for e in cc.check_form("f", f)))


class UrlTests(unittest.TestCase):
    def test_an_issue_url_is_parsed(self):
        t, q = cc.parse_issue_url(BASE + "/issues/new?template=mission.yml&amp;mission=Q-1&amp;title=Mission%3A+Q-1")
        self.assertEqual((t, q["mission"], q["title"]), ("mission.yml", "Q-1", "Mission: Q-1"))

    def test_other_urls_are_not_issue_links(self):
        self.assertIsNone(cc.parse_issue_url(BASE + "/blob/main/README.md"))


class MissionTests(unittest.TestCase):
    def test_an_open_mission_needs_its_take_link(self):
        import tempfile
        d = Path(tempfile.mkdtemp())
        (d / "en" / "missions").mkdir(parents=True)
        (d / "en" / "missions" / "index.html").write_text("<a href='?mission=Q-1'>x</a>")
        ms = [{"question": "Q-1", "state": "open"}, {"question": "Q-2", "state": "open"}, {"id": "FC-X", "recurring": True, "submit": {"url": "https://s"}}]
        errs = cc.check_missions(d, ms)
        self.assertEqual(len(errs), 2)
        self.assertTrue(any("Q-2" in e for e in errs) and any("FC-X" in e for e in errs))


if __name__ == "__main__":
    unittest.main()
