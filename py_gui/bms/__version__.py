VERSION = (0, 1, 0)
PRERELEASE = None  # alpha, beta or rc
REVISION = None


def generate_version(version, prerelease=None, revision=None):
    version_parts = [".".join(map(str, version))]
    if prerelease is not None:
        version_parts.append("-{}".format(prerelease))
    if revision is not None:
        version_parts.append(".{}".format(revision))
    return "".join(version_parts)


__title__ = "bms"
__description__ = "Neutron Controls Battery Management System CLI application"
__url__ = None
__version__ = generate_version(VERSION, prerelease=PRERELEASE, revision=REVISION)
__author__ = "Rob Milne"
__author_email__ = "rob.milne@neutroncontrols.com"
__license__ = None