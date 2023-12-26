import Head from "next/head";
import styles from "@/styles/Home.module.css";
import { useEffect, useState } from "react";

export default function Home() {
  const [desiredTemp, setDesiredTemp] = useState(0);
  const [status, setStatus] = useState({
    status: "BOOST_ON",
    heatmat: "ON",
    temperatures: { dough: 23, box: 32 },
  });

  useEffect(() => {
    const interval = setInterval(() => {
      fetch("http://proofuino.local/status")
        .then((response) => response.json())
        .then((data) => setStatus(data))
        .catch((error) => console.error(error));
    }, 2000);

    return () => clearInterval(interval);
  }, []);

  const handleSubmit = (event: any) => {
    //send post request to /temperature
    fetch("http://proofuino.local/temperature", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ desiredDoughTemperature: desiredTemp }),
    })
      .then((response) => response.json())
      .then((data) => {
        console.log("Success:", data);
      })
      .catch((error) => {
        console.error("Error:", error);
      });
    //prevent full page reload which is the default behavior
    event?.preventDefault();
  };

  return (
    <>
      <Head>
        <title>Proofuino</title>
      </Head>
      <main className={styles.spaContainer}>
        <div className={styles.header}>Proofuino</div>
        <div className={styles.infoGrid}>
          <div className={styles.infoBox}>
            <form onSubmit={handleSubmit} className={styles.form}>
              <label className={styles.label} htmlFor="desiredTemp">
                Desired Dough Temperature
              </label>
              <input
                className={styles.input}
                type="text"
                id="desiredTemp"
                value={desiredTemp}
                onChange={(e) => {
                  let n = Number(e.target.value);
                  if (Number.isSafeInteger(n)) {
                    setDesiredTemp(n);
                  }
                }}
              />
              <input
                type="submit"
                value="Submit"
                className={styles.submitButton}
              />
            </form>
          </div>
          <div className={styles.infoBox}>
            <div className={styles.infoTitle}>Status</div>
            <div className={styles.infoContent}>{status.status}</div>
          </div>
          <div className={styles.infoBox}>
            <div className={styles.infoTitle}>Heatmat</div>
            <div className={styles.infoContent}>{status.heatmat}</div>
          </div>
          <div className={styles.infoBox}>
            <div className={styles.infoTitle}>Dough Temperature</div>
            <div className={styles.infoContent}>
              {status.temperatures.dough}
            </div>
          </div>
          <div className={styles.infoBox}>
            <div className={styles.infoTitle}>Box Temperature</div>
            <div className={styles.infoContent}>{status.temperatures.box}</div>
          </div>
        </div>
      </main>
    </>
  );
}
