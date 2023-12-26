import Head from "next/head";
import styles from "@/styles/Home.module.css";
import { useEffect, useState } from "react";

export default function Home() {
  const [desiredTemp, setDesiredTemp] = useState(0);
  const [status, setStatus] = useState({
    state: "BOOST_ON",
    heatmat: "ON",
    temperatures: { dough: 0, box: 0 },
  });

  const fetchStatus = () => {
    fetch(`http://${window.location.host}/status`)
      .then((response) => response.json())
      .then((data) => {
        setDesiredTemp(data.temperatures.desiredDoughTemperature);
        setStatus(data);
      })
      .catch((error) => console.error(error));
  };
  useEffect(() => {
    fetchStatus();
    const interval = setInterval(fetchStatus, 10000);
    return () => clearInterval(interval);
  }, []);

  const handleSubmit = (event: any) => {
    //send post request to /temperature
    fetch(`http://${window.location.host}/temperature`, {
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
            <div className={styles.infoContent}>{status.state}</div>
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
