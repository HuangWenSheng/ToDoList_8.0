﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using System.IO;
using System.Reflection;
using System.Xml;
using System.Globalization;
using System.Security.Cryptography;
using System.Security.Cryptography.Xml;

namespace Utils
{
	class RhinoLicensing
	{
		static String PUBLIC_KEY = "<RSAKeyValue><Modulus>9twJpwt/Ofe58BOdK5Cb8XKGP5bvgxGh3IYkvCqvdzOCH3pi9BvOX+/fsRo/7HFbNmPr3Txu+hBl1JVH9ACXDxm20oKqgl6TzIk33iV6SrbuiZASi1OPAiTmsWBGKTIwrG9KiQ8JGmBotV/v2gRflqKELwiMUOO9W2DlgJ6szq0=</Modulus><Exponent>AQAB</Exponent></RSAKeyValue>";
		static String ALL_MODULES = "00000000-0000-0000-0000-000000000000";

		public enum LicenseType
		{
			Free,
			Trial,
			Paid,
			Supporter,
			Contributor,
		};
		
		public static LicenseType GetLicense(String typeId)
		{
			String licType;

			if (HasLicense(ALL_MODULES, out licType))
			{
				switch (licType)
				{
					case "Supported":	return LicenseType.Supporter;
					case "Contributor": return LicenseType.Contributor;
				}
			}
			else if (HasLicense(typeId, out licType))
			{
				switch (licType)
				{
					case "Free":		return LicenseType.Free;
					case "Paid":		return LicenseType.Paid;
				}
			}

			// all else
			return LicenseType.Trial;
		}

		private static bool HasLicense(String typeId, out String licType)
		{
			licType = String.Empty;

			String appFolder = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
			String licenseFolder = Path.Combine(appFolder, "Resources\\Licenses");

			if (Directory.Exists(licenseFolder))
			{
				var licenseFiles = Directory.EnumerateFiles(licenseFolder, typeId + ".xml", SearchOption.AllDirectories);
				var attributes = new Dictionary<String, String>();

				foreach (String licenseFile in licenseFiles)
				{
					if (!CheckLicense(PUBLIC_KEY, licenseFile, attributes))
						continue;

					// Validate attributes
					String licId, expiryDate, email, pluginName, pluginId;

					if (!attributes.TryGetValue("id", out licId) ||
						!attributes.TryGetValue("expiration", out expiryDate) ||
						!attributes.TryGetValue("type", out licType) ||
						!attributes.TryGetValue("email", out email) ||
						!attributes.TryGetValue("plugin_name", out pluginName) ||
						!attributes.TryGetValue("plugin_id", out pluginId))
					{
						continue;
					}

					if (!pluginId.Equals(typeId))
						continue;

					// TODO
					//var expirationDate = DateTime.ParseExact(date.Value, "yyyy-MM-ddTHH:mm:ss.fffffff", CultureInfo.InvariantCulture);
					//var licType = (/*LicenseType)Enum.Parse(typeof(LicenseType), */licenseType.Value);

					return true;
				}
			}


			return false;
		}

		public static bool CheckLicense(String publicKey, String licensePath, /*out*/ Dictionary<String, String> attributes)
		{
			if (String.IsNullOrEmpty(publicKey) || String.IsNullOrEmpty(licensePath) || !System.IO.File.Exists(licensePath))
				return false;

			var licenseText = File.ReadAllText(licensePath);

			var doc = new XmlDocument();
			doc.LoadXml(licenseText);

			var rsa = new RSACryptoServiceProvider();
			rsa.FromXmlString(publicKey);

			var nsMgr = new XmlNamespaceManager(doc.NameTable);
			nsMgr.AddNamespace("sig", "http://www.w3.org/2000/09/xmldsig#");

			var signedXml = new SignedXml(doc);
			var sig = (XmlElement)doc.SelectSingleNode("//sig:Signature", nsMgr);

			if (sig == null)
			{
				//Log.WarnFormat("Could not find this signature node on license:\r\n{0}", License);
				return false;
			}

			signedXml.LoadXml(sig);

			if (!signedXml.CheckSignature(rsa))
				return false;

			// Extract attributes
			var license = doc.SelectSingleNode("/license");
			int att = license.Attributes.Count;

			while (att-- > 0)
			{
				var attrib = license.Attributes[att];

				attributes[attrib.Name] = attrib.Value;
			}

			return true;
		}

	}
}