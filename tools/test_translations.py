"""Tests for hooks/translations.py: the link to the other language is computed, and offered only when the page exists."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "hooks"))
import translations as tr  # noqa: E402

PAGES = {"index.md", "fr/README.md", "en/README.md", "en/01-boot-chain.md", "fr/01-boot-chain.md", "en/bestiary.md", "fr/bestiaire.md",
         "en/campaigns/wifi.md", "fr/campagnes/wifi.md", "en/journey/start.md", "fr/journey/start.md", "en/missions.md"}


class CounterpartTests(unittest.TestCase):
    def test_pages_with_the_same_name(self):
        self.assertEqual(tr.counterpart("en/01-boot-chain.md", PAGES), "fr/01-boot-chain.md")
        self.assertEqual(tr.counterpart("fr/journey/start.md", PAGES), "en/journey/start.md")

    def test_the_bestiary_and_the_campaigns_have_french_names(self):
        self.assertEqual(tr.counterpart("en/bestiary.md", PAGES), "fr/bestiaire.md")
        self.assertEqual(tr.counterpart("fr/bestiaire.md", PAGES), "en/bestiary.md")
        self.assertEqual(tr.counterpart("en/campaigns/wifi.md", PAGES), "fr/campagnes/wifi.md")
        self.assertEqual(tr.counterpart("fr/campagnes/wifi.md", PAGES), "en/campaigns/wifi.md")

    def test_the_two_home_pages_point_at_each_other(self):
        self.assertEqual(tr.counterpart("index.md", PAGES), "fr/README.md")
        self.assertEqual(tr.counterpart("fr/README.md", PAGES), "index.md")

    def test_no_link_when_the_other_page_does_not_exist(self):
        self.assertIsNone(tr.counterpart("en/missions.md", PAGES))
        self.assertIsNone(tr.counterpart("en/03-hardware-map.md", PAGES))

    def test_pages_outside_the_two_languages_have_none(self):
        self.assertIsNone(tr.counterpart("research/index.md", PAGES))


if __name__ == "__main__":
    unittest.main()
